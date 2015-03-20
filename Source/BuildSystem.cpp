#include <iostream>
#include <map>
#include <queue>
#include <string>
#include <vector>
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>
#include "JSONTokenizer.hpp"
#include <fstream>
#include <sys/stat.h>

/*
Example input:

[{
	"cmd" : ["clang++", "-std=c++11", "-Wall", "-Wextra", "-o", "foo", "foo.cpp"],
	"dep" : ["foo.hpp", "foo.hpp"],
	"tgt" : ["foo"],
}]
*/

struct Job {
	std::vector<std::string> command;
	std::size_t outstandingDependencies = 0;
	std::vector<std::string> dependencies;
	std::vector<std::string> targets;
	// inverseDependencies are jobs that depend on us.
	std::vector<Job *> inverseDependencies;
	bool finished = false;
};

struct Engine {
	std::size_t runningJobCount = 0;
	std::size_t maximumJobCount = 4; // thang
	// readyToDispatchQueue is for jobs that are ready to go but may be waiting for a job slot to open up (job slots are limited by the core/CPU count of the machine).
	std::queue<Job *> readyToDispatchQueue;
	std::map<std::string, Job *> jobLookupByTarget;
	std::map<pid_t, Job *> jobLookupByPID;
};

std::string Join(const std::string &separator, const std::vector<std::string> &array) {
	std::string result;
	bool first = true;
	for (const std::string &string : array) {
		if (first)
			first = false;
		else
			result += separator;
		result += string;
	}
	return result;
}

bool operator<(struct timespec a, struct timespec b) {
	if (a.tv_sec < b.tv_sec) {
		return true;
	} else if (a.tv_sec > b.tv_sec) {
		return false;
	} else {
		return a.tv_nsec < b.tv_nsec;
	}
}

bool operator>=(struct timespec a, struct timespec b) {
	return !(a < b);
}

bool IsJobNeeded(Job *job) {
	// Commands with no targets are always run.
	if (job->targets.size() == 0)
		return true;

	// Find the most recently built target.
	struct timespec oldestTargetTime;
	bool oldestTargetTimeSet = false;
	for (const std::string &target : job->targets) {
		struct stat fileInfo;
		const int result = stat(target.c_str(), &fileInfo);
		if (result != 0) {
			// If a target does not exist we have to run the command.
			if (errno == ENOENT)
				return true;
			throw 0; // thang
		}
		// thang : also look at ctim?
		const timespec targetTime = fileInfo.st_mtim;
		if (oldestTargetTimeSet) {
			if (targetTime < oldestTargetTime)
				oldestTargetTime = targetTime;
		} else {
			oldestTargetTime = targetTime;
			oldestTargetTimeSet = true;
		}
	}

	assert(oldestTargetTimeSet);

	// If any dependency is newer than any target, rebuild.
	for (const std::string &dependency : job->dependencies) {
		struct stat fileInfo;
		const int result = stat(dependency.c_str(), &fileInfo);
		if (result != 0)
			throw 0; // thang
		// thang : also look at ctim?
		const struct timespec dependencyTime = fileInfo.st_mtim;
		if (dependencyTime >= oldestTargetTime) // thang: > or >=?
			return true;
	}

	return false;
}

void CreateDirectoryForTarget(const std::string target) {
	// thang : this directory is probably wrong...

	auto iterator = target.begin();
	auto lastSlashIterator = target.end();
	while (iterator != target.end()) {
		if (*iterator == '/') {
			lastSlashIterator = iterator;
		}
		++iterator; // thang : we could optimize by going backwards...
	}
	if (lastSlashIterator == target.end()) {
		// Target does not have a directory component.
		return;
	}
	std::string path(target.begin(), lastSlashIterator);
	struct stat fileInfo;
	if (stat(path.c_str(), &fileInfo) == 0) {
		// thang : check if it is a directory.
		return;
	}

	std::cout << "[DIR] Creating directory " << path << "\n";

	char lastCharacter = '/';
	for (auto iterator = target.begin(); iterator != target.end(); ++iterator) {
		if (*iterator == '/' && lastCharacter != '/') {
			std::string path(target.begin(), iterator);
			mkdir(path.c_str(), 0777); // thang : error check...
		}
		lastCharacter = *iterator;
	}
}

void CreateDirectoriesForJob(const Job *const job) {
	for (const std::string &target : job->targets) {
		CreateDirectoryForTarget(target);
	}
}

void BeginJob(Engine &engine, Job *job) {
	CreateDirectoriesForJob(job);

	std::cout << "[Begin] " << Join(" ", job->command) << "\n";

	const pid_t childPID = fork();
	if (childPID == 0) {
		// We are the child.
		// thang : set the environment?
		char **arguments = new char*[job->command.size() + 1];
		for (size_t n = 0; n < job->command.size(); ++n)
			arguments[n] = const_cast<char *>(job->command[n].c_str());
		arguments[job->command.size()] = nullptr;

		execvp(job->command[0].c_str(), arguments);
		throw 0; // thang
	} else if (childPID != -1) {
		// We are the parent.
		assert(engine.runningJobCount < engine.maximumJobCount);
		++engine.runningJobCount;
		engine.jobLookupByPID[childPID] = job;
	} else {
		// Error.
		throw 0; // thang
	}
}

void FinishJob(Engine &engine, Job *job) {
	job->finished = true;
	for (Job *job : job->inverseDependencies) {
		assert(job->outstandingDependencies != 0);
		--job->outstandingDependencies;
		if (job->outstandingDependencies == 0)
			engine.readyToDispatchQueue.push(job);
	}
}

// If 'wait' is true, it will block until all jobs are done.
void DispatchJobs(Engine &engine, const bool wait) {
	for (;;) {
		// Start some jobs that are in the queue.
		while (engine.runningJobCount < engine.maximumJobCount && !engine.readyToDispatchQueue.empty()) {
			Job *job = engine.readyToDispatchQueue.front();
			engine.readyToDispatchQueue.pop();
			if (IsJobNeeded(job))
				BeginJob(engine, job);
			else
				FinishJob(engine, job);
		}

		if (engine.runningJobCount == 0)
			return; // thang : is this deadlock or completion?

		int status;
		const int result = waitpid(-1, &status, wait ? 0 : WNOHANG); // thang : should first parameter be 0 or -1?
		if (result == 0) {
			// No child has exited.
			assert(!wait);
			return;
		} else if (result == -1) {
			// Error.
			throw 0; // thang
		} else {
			// A child has exited.
			if (WIFEXITED(status)) {
				const int exitStatus = WEXITSTATUS(status);
				if (exitStatus != 0)
					throw 0; // thang

				const auto iterator = engine.jobLookupByPID.find(result);
				if (iterator != engine.jobLookupByPID.end()) {
					--engine.runningJobCount;

					Job *finishedJob = iterator->second;
					engine.jobLookupByPID.erase(iterator);
					FinishJob(engine, finishedJob);
				}
			} else { // thang : test for more conditions than WIFEXITED?
				throw 0; // thang
			}
		}
	}
}

void EnqueueJob(Engine &engine, std::vector<std::string> command, std::vector<std::string> targets, std::vector<std::string> dependencies) {
	Job *job = new Job; // thang : leaks on throw

	if (command.size() == 0)
		throw 0; // thang
	job->command = std::move(command);

	for (const std::string &target : targets) {
		auto result = engine.jobLookupByTarget.insert(std::pair<std::string, Job *>(target, job));
		if (!result.second) { // gigathang : this doesn't look right at all.
			throw 0; // thang : duplicate target
		}
	}

	for (const std::string &dependency : dependencies) {
		// thang : check for cyclical dependencies?
		const auto iterator = engine.jobLookupByTarget.find(dependency);
		if (iterator != engine.jobLookupByTarget.end() && !iterator->second->finished) {
			++job->outstandingDependencies;
			iterator->second->inverseDependencies.push_back(job); // thang : check for duplicates?
		}
	}

	job->targets = std::move(targets);
	job->dependencies = std::move(dependencies);

	if (job->outstandingDependencies == 0)
		engine.readyToDispatchQueue.push(job);

	DispatchJobs(engine, false);
}

std::vector<std::string> ReadStringArray(JSONTokenizer &tokenizer) {
	std::vector<std::string> result;

	tokenizer.ReadRequiredToken(JSONTokenType::StartList);
	for (;;) {
		JSONToken token = tokenizer.ReadRequiredToken();
		if (token.GetType() == JSONTokenType::ListSeparator) // thang : don't make this check if this is the first loop iteration?
			token = tokenizer.ReadRequiredToken();

		if (token.GetType() == JSONTokenType::String) {
			result.push_back(token.GetValue());
		} else if (token.GetType() == JSONTokenType::EndList) {
			// thang : check that a separator was not discarded?
			return result;
		} else {
			throw std::runtime_error("Unexpected token.");
		}
	}
}

void ReadInput(InputStream &input) {
	Engine engine;

	JSONTokenizer tokenizer(input);

	tokenizer.ReadRequiredToken(JSONTokenType::StartList);
	for (;;) {
		JSONToken token = tokenizer.ReadRequiredToken();
		if (token.GetType() == JSONTokenType::ListSeparator) // thang : don't make this check if this is the first loop iteration?
			token = tokenizer.ReadRequiredToken();

		if (token.GetType() == JSONTokenType::StartObject) {
			std::vector<std::string> command;
			std::vector<std::string> targets;
			std::vector<std::string> dependencies;

			for (;;) {
				JSONToken token = tokenizer.ReadRequiredToken();
				if (token.GetType() == JSONTokenType::ListSeparator) // thang : don't make this check if this is the first loop iteration?
					token = tokenizer.ReadRequiredToken();

				if (token.GetType() == JSONTokenType::EndObject) {
					// thang : check that command is set?
					break;
				} else if (token.GetType() == JSONTokenType::String) {
					tokenizer.ReadRequiredToken(JSONTokenType::PairSeparator);
					if (token.GetValue() == "cmd") {
						command = ReadStringArray(tokenizer);
						// thang : make sure command length is non zero? and first argument has non-zero length?
					} else if (token.GetValue() == "tgt") {
						targets = ReadStringArray(tokenizer);
					} else if (token.GetValue() == "dep") {
						dependencies = ReadStringArray(tokenizer);
					} else {
						throw std::runtime_error("Unexpected token.");
					}
				} else {
					throw std::runtime_error("Unexpected token line " + std::to_string(token.GetLineNumber()));
				}
			}
			EnqueueJob(engine, std::move(command), std::move(targets), std::move(dependencies));
		} else if (token.GetType() == JSONTokenType::EndList) {
			// thang : check that a separator was not discarded?
			// thang : test if end of file?
			break;
		} else {
			throw std::runtime_error("Unexpected token.");
		}
	}

	DispatchJobs(engine, true);
}

int main() {
	InputStream input;
	input.file = popen("perl Buildfile.pl", "r"); // thang : error check...
	ReadInput(input);
	input.Close();

	return 0;
}
