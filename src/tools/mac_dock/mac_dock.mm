#include <Cocoa/Cocoa.h>

#include "mac_dock.h"

#include "privateqt_mac.h"

static NSInteger requestType = 0;

void MacDock::startBounce()
{
	requestType = [NSApp requestUserAttention:NSCriticalRequest];
}

void MacDock::stopBounce()
{
	if (requestType) {
		[NSApp cancelUserAttentionRequest:requestType];
		requestType = 0;
	}
}

void MacDock::overlay(const QString& text)
{
	NSString *str = (NSString *)CFBridgingRelease(QtCFString::toCFStringRef(text));
	[[NSApp dockTile] setBadgeLabel:str];
}
