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

class EventBus;

template <typename E>
class SubscriptionRegistry
{
public:
	using PreHandler = std::function<void(const EventBus *, E &)>;
	using PostHandler = std::function<void(const EventBus *, const E &)>;
	using BusTag = const void *;

	std::unique_ptr<Subscription> subscribeBefore(BusTag tag, PreHandler && cb)
	{
		auto storage = std::make_shared<PreHandlerStorage>(std::move(cb));
		preHandlers[tag].push_back(storage);
		return make_unique<PreSubscription>(tag, storage);
	}

	std::unique_ptr<Subscription> subscribeAfter(BusTag tag, PostHandler && cb)
	{
		auto storage = std::make_shared<PostHandlerStorage>(std::move(cb));
		postHandlers[tag].push_back(storage);
		return make_unique<PostSubscription>(tag, storage);
	}

	void executeEvent(const EventBus * bus, E & event)
	{
		{
			auto it = preHandlers.find(bus);

			if(it != std::end(preHandlers))
			{
				for(auto & h : it->second)
					(*h)(bus, event);
			}
		}

		event.internalExecute(bus);

		{
			auto it = postHandlers.find(bus);

			if(it != std::end(postHandlers))
			{
				for(auto & h : it->second)
					(*h)(bus, event);
			}
		}
	}

	static SubscriptionRegistry<E> * get()
	{
		static std::unique_ptr<SubscriptionRegistry<E>> Instance = make_unique<SubscriptionRegistry<E>>();
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

		void operator()(const EventBus * bus, E & event)
		{
			cb(bus, event);
		}
	private:
		T cb;
	};

	using PreHandlerStorage = HandlerStorage<PreHandler>;
	using PostHandlerStorage = HandlerStorage<PostHandler>;

	class PreSubscription : public Subscription
	{
	public:
		PreSubscription(BusTag tag_, std::shared_ptr<PreHandlerStorage> cb_)
			: cb(cb_),
			tag(tag_)
		{
		}

		virtual ~PreSubscription()
		{
			auto & handlers = SubscriptionRegistry<E>::get()->preHandlers;
			auto it = handlers.find(tag);

			if(it != std::end(handlers))
				it->second -= cb;
		}
	private:
		std::shared_ptr<PreHandlerStorage> cb;
		BusTag tag;
	};

	class PostSubscription : public Subscription
	{
	public:
		PostSubscription(BusTag tag_, std::shared_ptr<PostHandlerStorage> cb_)
			: cb(cb_),
			tag(tag_)
		{
		}

		virtual ~PostSubscription()
		{
			auto & handlers = SubscriptionRegistry<E>::get()->postHandlers;
			auto it = handlers.find(tag);

			if(it != std::end(handlers))
				it->second -= cb;
		}
	private:
		BusTag tag;
		std::shared_ptr<PostHandlerStorage> cb;
	};

	std::map<BusTag, std::vector<std::shared_ptr<PreHandlerStorage>>> preHandlers;
	std::map<BusTag, std::vector<std::shared_ptr<PostHandlerStorage>>> postHandlers;
};

class EventExample
{
public:
	using PreHandler = SubscriptionRegistry<EventExample>::PreHandler;
	using PostHandler = SubscriptionRegistry<EventExample>::PostHandler;
	using BusTag = SubscriptionRegistry<EventExample>::BusTag;

public:
	MOCK_METHOD1(internalExecuteStub, void(const EventBus *));

private:

	void internalExecute(const EventBus * bus)
	{
		internalExecuteStub(bus);
	}

	friend class SubscriptionRegistry<EventExample>;
};

class EventBus
{
public:
	template <typename E>
	std::unique_ptr<Subscription> subscribeBefore(typename E::PreHandler && cb)
	{
		auto registry = SubscriptionRegistry<E>::get();
		return registry->subscribeBefore(this, std::move(cb));
	}

	template <typename E>
	std::unique_ptr<Subscription> subscribeAfter(typename E::PostHandler && cb)
	{
		auto registry = SubscriptionRegistry<E>::get();
		return registry->subscribeAfter(this, std::move(cb));
	}

	template <typename E>
	void executeEvent(E & event) const
	{
		SubscriptionRegistry<E>::get()->executeEvent(this, event);
	}
};

class ListenerMock
{
public:
	MOCK_METHOD2(beforeEvent, void(const EventBus *, EventExample &));
	MOCK_METHOD2(afterEvent, void(const EventBus *, const EventExample &));
};

class EventBusTest : public Test
{
public:
	EventExample event1;
	EventExample event2;
	EventBus subject1;
	EventBus subject2;
};

TEST_F(EventBusTest, ExecuteNoListeners)
{
	EXPECT_CALL(event1, internalExecuteStub(_)).Times(1);
	subject1.executeEvent(event1);
}

TEST_F(EventBusTest, ExecuteIgnoredSubscription)
{
	StrictMock<ListenerMock> listener;

	subject1.subscribeBefore<EventExample>(std::bind(&ListenerMock::beforeEvent, &listener, _1, _2));
	subject1.subscribeAfter<EventExample>(std::bind(&ListenerMock::afterEvent, &listener, _1, _2));

	EXPECT_CALL(listener, beforeEvent(_,_)).Times(0);
	EXPECT_CALL(event1, internalExecuteStub(_)).Times(1);
	EXPECT_CALL(listener, afterEvent(_,_)).Times(0);

	subject1.executeEvent(event1);
}

TEST_F(EventBusTest, ExecuteSequence)
{
	StrictMock<ListenerMock> listener1;
	StrictMock<ListenerMock> listener2;

	auto subscription1 = subject1.subscribeBefore<EventExample>(std::bind(&ListenerMock::beforeEvent, &listener1, _1, _2));
	auto subscription2 = subject1.subscribeAfter<EventExample>(std::bind(&ListenerMock::afterEvent, &listener1, _1, _2));
	auto subscription3 = subject1.subscribeBefore<EventExample>(std::bind(&ListenerMock::beforeEvent, &listener2, _1, _2));
	auto subscription4 = subject1.subscribeAfter<EventExample>(std::bind(&ListenerMock::afterEvent, &listener2, _1, _2));

	{
		InSequence local;
		EXPECT_CALL(listener1, beforeEvent(Eq(&subject1), Ref(event1))).Times(1);
		EXPECT_CALL(listener2, beforeEvent(Eq(&subject1), Ref(event1))).Times(1);
		EXPECT_CALL(event1, internalExecuteStub(_)).Times(1);
		EXPECT_CALL(listener1, afterEvent(Eq(&subject1), Ref(event1))).Times(1);
		EXPECT_CALL(listener2, afterEvent(Eq(&subject1), Ref(event1))).Times(1);
	}

	subject1.executeEvent(event1);
}

TEST_F(EventBusTest, BusesAreIndependent)
{
	StrictMock<ListenerMock> listener1;
	StrictMock<ListenerMock> listener2;

	auto subscription1 = subject1.subscribeBefore<EventExample>(std::bind(&ListenerMock::beforeEvent, &listener1, _1, _2));
	auto subscription2 = subject1.subscribeAfter<EventExample>(std::bind(&ListenerMock::afterEvent, &listener1, _1, _2));
	auto subscription3 = subject2.subscribeBefore<EventExample>(std::bind(&ListenerMock::beforeEvent, &listener2, _1, _2));
	auto subscription4 = subject2.subscribeAfter<EventExample>(std::bind(&ListenerMock::afterEvent, &listener2, _1, _2));

	EXPECT_CALL(listener1, beforeEvent(_, _)).Times(1);
	EXPECT_CALL(listener2, beforeEvent(_, _)).Times(0);
	EXPECT_CALL(event1, internalExecuteStub(_)).Times(1);
	EXPECT_CALL(listener1, afterEvent(_, _)).Times(1);
	EXPECT_CALL(listener2, afterEvent(_, _)).Times(0);

	subject1.executeEvent(event1);
}

}
