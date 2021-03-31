#include "clara_event_source.hpp"

ClaraEventSource::ClaraEventSource(std::string res_name, JApplication* app) :
		JEventSource(std::move(res_name), app) {
	// TODO: Get EventGroupManager from ServiceLocator instead
	m_pending_group_id = 1;
}

void ClaraEventSource::SubmitAndWait(std::vector<SampaEvent*>& events) {
	auto group = m_egm.GetEventGroup(m_pending_group_id++);
	{
		std::lock_guard<std::mutex> lock(m_pending_mutex);
		for (auto event : events) {
			group->StartEvent();   // We have to call this immediately in order to 'open' the group
			m_pending_events.push(std::make_pair(event, group));
		}
	}
	group->CloseGroup();
	group->WaitUntilGroupFinished();
}

void ClaraEventSource::GetEvent(std::shared_ptr<JEvent> event) {

	std::pair<SampaEvent*, JEventGroup*> next_event;
	{
		std::lock_guard<std::mutex> lock(m_pending_mutex);
		if (m_pending_events.empty()) {
			throw RETURN_STATUS::kTRY_AGAIN;
		} else {
			next_event = m_pending_events.front();
			m_pending_events.pop();
		}
	}

	// Hydrate JEvent with both the SampaEvent and the group pointer.
	event->Insert(next_event.first);    // SampaEvent
	event->Insert(next_event.second);   // JEventGroup

	// Tell JANA not to assume ownership of these objects!
	event->GetFactory<SampaEvent>()->SetFactoryFlag(JFactory::JFactory_Flags_t::NOT_OBJECT_OWNER);
	event->GetFactory<JEventGroup>()->SetFactoryFlag(JFactory::JFactory_Flags_t::NOT_OBJECT_OWNER);

	// JANA always needs an event number and a run number, so extract these from the Tridas data somehow
	event->SetEventNumber(next_event.first->event_number);
	event->SetRunNumber(next_event.first->run_number);
}

