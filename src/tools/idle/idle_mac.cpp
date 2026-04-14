/*
 * idle_mac.cpp - detect desktop idle time
 * Copyright (C) 2003  Tarkvara Design Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "idle.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

IdlePlatform::IdlePlatform() {
}

IdlePlatform::~IdlePlatform() {
}

bool IdlePlatform::init() {
	return true;
}

// copied from Pidgin - gtkidle.c
int IdlePlatform::secondsIdle()
{
	static io_service_t macIOsrvc = NULL;
	CFTypeRef property;
	uint64_t idle_time = 0; // nanoseconds

	if (!macIOsrvc) {
		mach_port_t master;
		IOMasterPort(MACH_PORT_NULL, &master);
		macIOsrvc = IOServiceGetMatchingService(master,
		                                        IOServiceMatching("IOHIDSystem"));
	}

	property = IORegistryEntryCreateCFProperty(macIOsrvc, CFSTR("HIDIdleTime"),
	           kCFAllocatorDefault, 0);
	CFNumberGetValue((CFNumberRef)property,
	                 kCFNumberSInt64Type, &idle_time);
	CFRelease(property);

	// convert nanoseconds to seconds
	int seconds_idle = idle_time / 1000000000;
	return seconds_idle;
}
