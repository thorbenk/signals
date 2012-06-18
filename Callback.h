#ifndef SIGNALS_CALLBACK_H__
#define SIGNALS_CALLBACK_H__

#include <cassert>

#include <boost/function.hpp>

#include <util/typename.h>
#include "Slot.h"
#include "Logging.h"

namespace signals {

class CallbackBase {

public:

	/**
	 * Comparison operator that sorts callbacks according to their specificity:
	 *
	 * Callback<Derived> < Callback<Base>
	 */
	bool operator<(const CallbackBase& other) const {

		LOG_ALL(signalslog) << "comparing callback " << typeName(*this) << " with "
		                    << typeName(other) << "..." << std::endl;
		LOG_ALL(signalslog) << "I am " << (other.accepts(createSignal()) ? "smaller" : "bigger")
		                    << std::endl;

		// Return true, if the other callback accepts our signals as well. This
		// means that our signal type ≤ other signal type, i.e., we are more
		// specific.
		return other.accepts(createSignal());
	}

	/**
	 * Try to connect to the given slot. For a successful connection, the slot's
	 * signal type has to be castable to the callbacks signal type.
	 *
	 * @return True, if the connection could be established.
	 */
	virtual bool tryToConnect(SlotBase& slot) = 0;

protected:

	/**
	 * Return true, if the passed signal can be cast to the signal type this
	 * callback accepts.
	 */
	virtual bool accepts(const Signal& signal) const = 0;

	/**
	 * Create a reference signal for run-time type inference. This reference is
	 * used to find compatible pairs of slots and callbacks.
	 */
	virtual const Signal& createSignal() const = 0;
};

struct CallbackComparator {

	bool operator()(const CallbackBase* a, const CallbackBase* b) const {

		return *a < *b;
	}
};

class NoTracking {

protected:

	template <typename SignalType>
	boost::function<void(SignalType&)> wrap(boost::function<void(SignalType&)> callback) const {

		return callback;
	}
};

template <typename HolderType>
class WeakTracking {

public:

	void track(boost::shared_ptr<HolderType> holder) const {

		_holder = holder;
	}

protected:

	template <typename SignalType>
	boost::function<void(SignalType&)> wrap(boost::function<void(SignalType&)> callback) {

		typedef boost::signals2::signal<void(SignalType&)> boost_signal_type;

		boost::shared_ptr<HolderType> sharedHolder = _holder.lock();

		return typename boost_signal_type::slot_type(callback).track(sharedHolder);
	}

private:

	mutable boost::weak_ptr<HolderType> _holder;
};

template <typename ToType>
class StaticCast {

public:

	template <typename FromType>
	ToType cast(FromType& from) {

		return static_cast<ToType>(from);
	}
};

template <
	typename SignalType,
	class TrackingPolicy = NoTracking,
	template <typename ToType> class CastingPolicy = StaticCast>
class Callback : public CallbackBase, public TrackingPolicy, public CastingPolicy<SignalType&> {

public:

	typedef boost::function<void(SignalType&)> callback_type;

	Callback(callback_type callback) : _callback(callback) {}

	bool accepts(const Signal& signal) const {

		return dynamic_cast<const SignalType*>(&signal);
	}

	bool tryToConnect(SlotBase& slot) {

		const Signal& reference = slot.createSignal();

		// reference ≤ SignalType
		if (accepts(reference)) {

			LOG_ALL(signalslog) << typeName(*this) << ": I can handle signal "
			                    << typeName(reference) << "!" << std::endl;

			/* Here, we cast whatever comes to Slot<SignalType>. This means,
			 * that we every once in a while cast Slot<Dervied> to Slot<Base>
			 * (not that the Slots themselves are not dervied from each other).
			 *
			 * What can go wrong?
			 *
			 * The only thing we do is to call connect(Callback<Base>), where
			 * instead the object implements connect(Callback<Derived>). In
			 * connect, we take the actual boost::signal and connect
			 * Callback<Base>::operator()(Signal&) to it.
			 *
			 * Since all the Slot<SignalType>s are identical in memory (we know
			 * this since we made them), the static casts are safe. We will talk
			 * to the right object with the wrong name, but this doesn't matter,
			 * since the all have a boost::signal of the same name -- and there
			 * everything is save again.
			 */
			Slot<SignalType>& s = static_cast<Slot<SignalType>&>(slot);

			s.connect(TrackingPolicy::wrap(boost::function<void(SignalType&)>(*this)));

			return true;

		} else {

			LOG_ALL(signalslog) << typeName(*this) << ": I cannot handle signal "
			                    << typeName(reference) << std::endl;

			return false;
		}
	}

	/* The point of this method is to cast the signal -- that was transmitted as
	 * a Signal -- to SignalType (where SignalType can be ≥ than the actual type
	 * of the signal).
	 */
	void operator()(Signal& signal) {

		SignalType& casted = CastingPolicy<SignalType&>::cast(signal);

		_callback(casted);
	}

	/* The point of this method is to cast the signal -- that was transmitted as
	 * a Signal -- to SignalType (where SignalType can be ≥ than the actual type
	 * of the signal).
	 */
	void operator()(const Signal& signal) {

		const SignalType& casted = CastingPolicy<SignalType&>::cast(signal);

		_callback(casted);
	}

protected:

	const Signal& createSignal() const {

		return Slot<SignalType>::referenceSignal;
	}

private:

	callback_type _callback;
};

} // namespace signals

#endif // SIGNALS_CALLBACK_H__

