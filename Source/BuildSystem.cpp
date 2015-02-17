#include <iostream>
#include <map>
#include <queue>
#include <string>
#include <vector>
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>

struct Job {
	std::vector<std::string> command;
	std::size_t outstandingDependencies = 0;
	std::vector<Job *> inverseDependencies;
	bool finished = false;
};

struct Engine {
	std::size_t runningJobCount = 0;
	std::size_t maximumJobCount = 4; // thang
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
			return;
		} else if (result == -1) {
			// Error.
			throw 0;
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
			} else {
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
		if (!result.second) {
			throw 0; // thang : duplicate target
		}
	}

	for (const std::string &dependency : dependencies) {
		const auto iterator = engine.jobLookupByTarget.find(dependency);
		if (iterator == engine.jobLookupByTarget.end())
			throw 0; // thang : dependency not found.

		if (!iterator->second->finished) {
			++job->outstandingDependencies;
			iterator->second->inverseDependencies.push_back(job); // thang : check for duplicates?
		}
	}

	if (job->outstandingDependencies == 0)
		engine.readyToDispatchQueue.push(job);

	DispatchJobs(engine, false);
}

int main() {
	Engine engine;
	EnqueueJob(engine, {"bash", "-c", "sleep 2 && echo a"}, {"a"}, {});
	EnqueueJob(engine, {"bash", "-c", "sleep 2 && echo b"}, {"b"}, {"a"});
	EnqueueJob(engine, {"bash", "-c", "sleep 2 && echo c"}, {"c"}, {"a"});
	EnqueueJob(engine, {"bash", "-c", "sleep 2 && echo d"}, {"d"}, {"b", "c"});
	DispatchJobs(engine, true);

	return 0;
}
