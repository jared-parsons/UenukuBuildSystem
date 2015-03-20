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

void BeginJob(Engine &engine, Job *job) {
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

void DispatchJobs(Engine &engine, const bool wait) {
	for (;;) {
		// Start some jobs that are in the queue.
		while (engine.runningJobCount < engine.maximumJobCount && !engine.readyToDispatchQueue.empty()) {
			Job *job = engine.readyToDispatchQueue.front();
			engine.readyToDispatchQueue.pop();
			BeginJob(engine, job);
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
					finishedJob->finished = true;
					for (Job *job : finishedJob->inverseDependencies) {
						assert(job->outstandingDependencies != 0);
						--job->outstandingDependencies;
						if (job->outstandingDependencies == 0)
							engine.readyToDispatchQueue.push(job);
					}
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

	for (std::string &target : targets) {
		auto result = engine.jobLookupByTarget.insert(std::pair<std::string, Job *>(std::move(target), job));
		if (!result.second) { // gigathang : this doesn't look right at all.
			throw 0; // thang : duplicate target
		}
	}

	for (const std::string &dependency : dependencies) {
		const auto iterator = engine.jobLookupByTarget.find(dependency);
		if (iterator == engine.jobLookupByTarget.end())
			throw 0; // thang : dependency not found. // gigathang : shouldn't we check if the file exists if the dependency was not found in the job list?

		// thang : check for cyclical dependencies?

		if (!iterator->second->finished) {
			++job->outstandingDependencies;
			iterator->second->inverseDependencies.push_back(job); // thang : check for duplicates?
		}
	}

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
