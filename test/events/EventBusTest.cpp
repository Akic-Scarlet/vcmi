/*
 * events\EventBusTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

namespace test
{
using namespace ::testing;

class Subscription : public boost::noncopyable
{
public:
	virtual ~Subscription() = default;
};


template <typename Event>
class SubscriptionRegistry
{
public:
	using PreHandler = std::function<void(Event &)>;
	using PostHandler = std::function<void(const Event &)>;


	std::unique_ptr<Subscription> subscribeBefore(PreHandler && cb)
	{
		auto storage = std::make_shared<PreHandlerStorage>(std::move(cb));
		preHandlers.push_back(storage);
		return make_unique<PreSubscription>(storage);
	}

	std::unique_ptr<Subscription> subscribeAfter(PostHandler && cb)
	{
		auto storage = std::make_shared<PostHandlerStorage>(std::move(cb));
		postHandlers.push_back(storage);
		return make_unique<PostSubscription>(storage);
	}

	void executeEvent(Event * event)
	{
		for(auto & h : preHandlers)
			(*h)(event);

		event->internalExecute();

		for(auto & h : postHandlers)
			(*h)(event);
	}

	static SubscriptionRegistry<Event> * get()
	{
		static std::unique_ptr<SubscriptionRegistry<Event>> Instance = make_unique<SubscriptionRegistry<Event>>();
		return Instance.get();
	}

private:

	template <typename T>
	class HandlerStorage
	{
	public:
		explicit HandlerStorage(T && cb_)
			: cb(cb_)
		{
		}

		void operator()(Event * event)
		{
			cb(*event);
		}
	private:
		T cb;
	};

	using PreHandlerStorage = HandlerStorage<PreHandler>;
	using PostHandlerStorage = HandlerStorage<PostHandler>;

	class PreSubscription : public Subscription
	{
	public:
		PreSubscription(std::shared_ptr<PreHandlerStorage> cb_)
			: cb(cb_)
		{
		}

		virtual ~PreSubscription()
		{
			auto & v = SubscriptionRegistry<Event>::get()->preHandlers;
			v -= cb;
		}
	private:
		std::shared_ptr<PreHandlerStorage> cb;
	};

	class PostSubscription : public Subscription
	{
	public:
		PostSubscription(std::shared_ptr<PostHandlerStorage> cb_)
			: cb(cb_)
		{
		}

		virtual ~PostSubscription()
		{
			auto & v = SubscriptionRegistry<Event>::get()->postHandlers;
			v -= cb;
		}
	private:
		std::shared_ptr<PostHandlerStorage> cb;
	};

	std::vector<std::shared_ptr<PreHandlerStorage>> preHandlers;
	std::vector<std::shared_ptr<PostHandlerStorage>> postHandlers;
};

class EventExample
{
public:
	using PreHandler = SubscriptionRegistry<EventExample>::PreHandler;
	using PostHandler = SubscriptionRegistry<EventExample>::PostHandler;

	static std::unique_ptr<Subscription> subscribeBefore(PreHandler && cb)
	{
		auto registry = SubscriptionRegistry<EventExample>::get();

		return registry->subscribeBefore(std::move(cb));
	}

	static std::unique_ptr<Subscription> subscribeAfter(PostHandler && cb)
	{
		auto registry = SubscriptionRegistry<EventExample>::get();

		return registry->subscribeAfter(std::move(cb));
	}

	void execute()
	{
		SubscriptionRegistry<EventExample>::get()->executeEvent(this);
	}

	MOCK_METHOD0(internalExecute, void());
};

class ListenerMock
{
public:
	MOCK_METHOD1(beforeEvent, void(EventExample &));
	MOCK_METHOD1(afterEvent, void(const EventExample &));
};

class EventBusTest : public Test
{

};

TEST_F(EventBusTest, ExecuteNoListeners)
{
	EventExample event;
	EXPECT_CALL(event, internalExecute()).Times(1);
	event.execute();
}

TEST_F(EventBusTest, ExecuteIgnoredSubscription)
{
	StrictMock<ListenerMock> listener;

	EventExample event;

	EventExample::subscribeBefore(std::bind(&ListenerMock::beforeEvent, &listener, _1));
	EventExample::subscribeAfter(std::bind(&ListenerMock::afterEvent, &listener, _1));

	EXPECT_CALL(listener, beforeEvent(_)).Times(0);
	EXPECT_CALL(event, internalExecute()).Times(1);
	EXPECT_CALL(listener, afterEvent(_)).Times(0);

	event.execute();
}

TEST_F(EventBusTest, ExecuteSequence)
{
	StrictMock<ListenerMock> listener1;
	StrictMock<ListenerMock> listener2;

	EventExample event;

	auto subscription1 = EventExample::subscribeBefore(std::bind(&ListenerMock::beforeEvent, &listener1, _1));
	auto subscription2 = EventExample::subscribeAfter(std::bind(&ListenerMock::afterEvent, &listener1, _1));
	auto subscription3 = EventExample::subscribeBefore(std::bind(&ListenerMock::beforeEvent, &listener2, _1));
	auto subscription4 = EventExample::subscribeAfter(std::bind(&ListenerMock::afterEvent, &listener2, _1));

	{
		InSequence local;
		EXPECT_CALL(listener1, beforeEvent(Ref(event))).Times(1);
		EXPECT_CALL(listener2, beforeEvent(Ref(event))).Times(1);
		EXPECT_CALL(event, internalExecute()).Times(1);
		EXPECT_CALL(listener1, afterEvent(Ref(event))).Times(1);
		EXPECT_CALL(listener2, afterEvent(Ref(event))).Times(1);
	}

	event.execute();
}

}
