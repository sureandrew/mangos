/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "EventProcessor.h"

EventProcessor::EventProcessor()
{
    m_time = 0;
    m_aborting = false;
}

EventProcessor::~EventProcessor()
{
    KillAllEvents(true);
}

void EventProcessor::Update(uint32 p_time, bool force)
{
    if (force)
        RenewEvents();

    // update time
    m_time += p_time;

    for (EventList::iterator itr = m_events.begin(); itr != m_events.end(); ++itr)
    {
        if (itr->first > m_time)
            continue;

        // get and remove event from queue
        BasicEvent* Event = itr->second;
        if (!Event)
            continue;

        // remove pointer - container be cleared in locked cycle
        itr->second = NULL;

        if (!Event->to_Abort)
        {
            if (Event->Execute(m_time, p_time))
            {
                // completely destroy event if it is not re-added
                delete Event;
            }
        }
        else
        {
            Event->Abort(m_time);
            delete Event;
        }
    }
}

void EventProcessor::KillAllEvents(bool force)
{
    // prevent event insertions
    m_aborting = true;

    if (force)
        RenewEvents();

    if (m_events.empty())
        return;

    // first, abort all existing events
    for (EventList::iterator itr = m_events.begin(); itr != m_events.end(); ++itr)
    {
        BasicEvent* Event = itr->second;
        if (!Event)
            continue;

        Event->to_Abort = true;
        Event->Abort(m_time);
        if (force || Event->IsDeletable())
        {
            // remove pointer - container be cleared in locked cycle
            itr->second = NULL;
            delete Event;
        }
    }

    // fast clear event list (in force case)
    if (force)
        m_events.clear();
}

void EventProcessor::AddEvent(BasicEvent* Event, uint64 e_time, bool set_addtime)
{
    if (m_aborting)
    {
        if (Event)
            delete Event;
        return;
    };

    if (set_addtime)
        Event->m_addTime = m_time;

    Event->m_execTime = e_time;
    m_queue.push(std::pair<uint64, BasicEvent*>(e_time, Event));
}

void EventProcessor::RenewEvents()
{
    if (!m_aborting && !m_events.empty())
    {
        for (EventList::iterator itr = m_events.begin(); itr != m_events.end();)
        {
            BasicEvent* Event = itr->second;
            if (!Event)
                m_events.erase(itr++);
            else
                ++itr;
        }
    }

    while (!m_queue.empty())
    {
        if (m_aborting)
        {
            BasicEvent* Event = m_queue.front().second;
            if (Event)
                delete Event;
        }
        else
            m_events.insert(m_queue.front());
        m_queue.pop();
    }
}

uint64 EventProcessor::CalculateTime(uint64 t_offset)
{
    return m_time + t_offset;
}
