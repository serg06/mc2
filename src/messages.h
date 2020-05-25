#include "messaging.h"
#include "vmath.h"

#include <cstddef> // std::size_t

// Messages that others can send to each other using pubsub
struct MsgMeshGenerated : pubsub::Message<vmath::ivec3>
{
};

struct MsgrMeshGenerated : pubsub::Messenger<vmath::ivec3>
{
};

// These two things should match
constexpr int num_message_types = 3;
enum class MessageType : std::size_t
{
	gen_mesh_request,
	mesh_generated,
	destroy_block
};

// Message context shared by everyone
class MessagingContext
{
public:
	template<MessageType T, typename MessageData>
	void post_message(const MessageData& data)
	{
		static_assert(T < sizeof(messengers) / sizeof(messengers[0]));
		MessageData* ptr = new MessageData(data);

	}

	template<MessageType T>
	void post_message()
	{
		messengers[]
	}

	template<MessageType T>
	void set_listener()
	{
		messengers[T]
	}

private:
	std::array<std::atomic<pubsub::Messenger<void*>*>, num_message_types> messengers;
};