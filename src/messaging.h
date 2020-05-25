#include <assert.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

// TODO: Try a C++ message broker library, like ZeroMQ?
namespace pubsub
{

template<typename T>
struct Message
{
	Message(T data_, Message<T>* next_) : data(data_), next(next_)
	{
	}

	T data;
	Message<T>* next;
};

template<typename T>
class Messenger
{
public:
	Messenger()
	{
	}

	// Post a message super quickly, no locks
	void post_message(T data)
	{
		// can be called from any thread
		// atomically post the message to the queue
		Message<T>* message = new Message<T>(data, nullptr);
		Message<T>* stale_head = messages.load(std::memory_order_relaxed);
		do
		{
			message->next = stale_head;
		} while( !messages.compare_exchange_weak( stale_head, message, std::memory_order_release ) );

		// notify if anyone's listening
		if(!stale_head) {
			std::unique_lock<std::mutex> lock(message_ready_mutex);
			message_ready.notify_one();
		}
	}

	template<const bool block>
	std::vector<T> pop_messages()
	{
		// Pop
		Message<T>* pending = messages.exchange(nullptr, std::memory_order_consume);
		if constexpr (block)
		{
			// Wait for messages
			if (!pending)
			{
				std::unique_lock<std::mutex> lock(message_ready_mutex);                
				// check one last time while holding the lock before blocking.
				if(!messages)
				{
					message_ready.wait(lock);
				}
				pending = messages.exchange(nullptr, std::memory_order_consume);
				assert(pending && "wtf");
			}
		}

		// Done, now reverse them and stick them in a vector for easy processing
		std::vector<T> result;
		if (pending != nullptr)
		{
			// Reverse
			Message<T>* prev = nullptr;
			Message<T>* curr = pending;
			while (curr != nullptr)
			{
				Message<T>* next = curr->next;
				curr->next = prev;
				prev = curr;
				curr = next;
			}
			pending = prev;

			// Stick into vector
			for (curr = pending; curr != nullptr; curr = curr->next)
			{
				result.push_back(curr->data);
			}
		}
		return result;
	}

private:
	std::atomic<Message<T>*> messages;

	// notify when message ready
	std::mutex message_ready_mutex;
	std::condition_variable message_ready;
};

}
