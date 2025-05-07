#pragma once

#include "ComponentArray.h"
#include "Utilities\JobSystem.h"

#include <functional>
#include <bitset>
#include <mutex>

namespace HBL2
{
	using ComponentMaskType = std::bitset<MAX_COMPONENT_TYPES>;

	struct SystemEntry
	{
		ComponentMaskType ReadMask;
		ComponentMaskType WriteMask;
		std::function<void()> Task;
	};

	class Sceduler
	{
	public:
		void Register(SystemEntry& entry)
		{
			m_Entries.push_back(std::move(entry));
		}

		void RunAll()
		{
			std::vector<std::vector<SystemEntry>> batches;

			ComponentMaskType batchReadMask;
			ComponentMaskType batchWriteMask;

			batches.emplace_back();
			std::vector<SystemEntry>* currentBatch = &batches.back();

			for (const auto& sys : m_Entries)
			{
				// Check conflicts with current batch.
				bool conflict =
					(sys.WriteMask & batchWriteMask).any() ||  // WW conflict
					(sys.WriteMask & batchReadMask).any() ||   // WR conflict
					(sys.ReadMask & batchWriteMask).any();     // RW conflict

				if (conflict)
				{
					// Start new batch
					batches.emplace_back();
					batchReadMask.reset();
					batchWriteMask.reset();
					currentBatch = &batches.back();
				}

				// Add system to current batch.
				currentBatch->push_back(sys);

				// Update read and write mask for current batch.
				batchReadMask |= sys.ReadMask;
				batchWriteMask |= sys.WriteMask;
			}

			// Execute batches
			for (const auto& batch : batches)
			{
				if (batch.size() == 1)
				{
					batch[0].Task();
				}
				else
				{
					JobContext ctx;
					for (const auto& sys : batch)
					{
						JobSystem::Get().Execute(ctx, sys.Task);
					}
					JobSystem::Get().Wait(ctx);
				}
			}
		}


	private:
		std::mutex mu;
		std::condition_variable cv;
		JobContext m_JobContext;
		std::vector<SystemEntry> m_Entries;
	};
}