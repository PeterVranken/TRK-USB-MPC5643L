#ifndef TC16_APPLEVENTS_INCLUDED
#define TC16_APPLEVENTS_INCLUDED
/**
 * @file tc16_applEvents.h
 * Definition of application events. The application events are managed in a
 * central file to avoid inconsistencies and accidental double usage.
 *
 * Copyright (C) 2012-2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Include files
 */


/*
 * Defines
 */

/** Task idTaskNonCyclic is always waiting for this event and triggers its action on
    reception. */
#define EVT_ACTIVATE_TASK_NON_CYCLIC        (RTOS_EVT_EVENT_00)

/** Task idTaskOnButtonDown is always waiting for this event and triggers its action on
    reception. */
#define EVT_ACTIVATE_TASK_ON_BUTTON_DOWN    (RTOS_EVT_EVENT_01)

/** Task idTask17ms is always waiting for this event and triggers its regular action
    prematurely on reception. */
#define EVT_ACTIVATE_TASK_17_MS             (RTOS_EVT_EVENT_02)


/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */



#endif  /* TC16_APPLEVENTS_INCLUDED */
