#include <assert.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

// TODO: Try a C++ message broker library, like ZeroMQ?
namespace pubsub
{

// Note: This is made with T being a pointer in mind.
template<typename T>
struct Message
{
	T data;
};

template<typename T>
struct MessageNode
{
	MessageNode(Message<T>* message_, MessageNode<T>* next_) : message(message_), next(next_)
	{
	}

	Message<T>* message;
	MessageNode<T>* next;
};

template<typename T>
class Messenger
{
public:
	Messenger()
	{
	}

	// Post a message super quickly, no locks
	void post_message(Message<T>* message)
	{
		// can be called from any thread
		// atomically post the message to the queue
		MessageNode<T>* link = new MessageNode<T>(message, nullptr);
		MessageNode<T>* stale_head = messages.load(std::memory_order_relaxed);
		do
		{
			link->next = stale_head;
		} while( !messages.compare_exchange_weak( stale_head, link, std::memory_order_release ) );

		// notify if anyone's listening
		if(!stale_head) {
			std::unique_lock<std::mutex> lock(message_ready_mutex);
			message_ready.notify_one();
		}
	}

	template<const bool block>
	std::vector<Message<T>*> pop_messages()
	{
		// Pop
		MessageNode<T>* pending = messages.exchange(nullptr, std::memory_order_consume);
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
		std::vector<Message<T>*> result;
		if (pending != nullptr)
		{
			// Reverse
			MessageNode<T>* prev = nullptr;
			MessageNode<T>* curr = pending;
			while (curr != nullptr)
			{
				MessageNode<T>* next = curr->next;
				curr->next = prev;
				prev = curr;
				curr = next;
			}
			pending = prev;

			// Stick into vector
			for (curr = pending; curr != nullptr; curr = curr->next)
			{
				result.push_back(curr->message);
			}
		}
		return result;
	}

private:
	std::atomic<MessageNode<T>*> messages;

	// notify when message ready
	std::mutex message_ready_mutex;
	std::condition_variable message_ready;
};

}
