#!/usr/bin/env python3
"""Exercise two MCP sessions and resource subscriptions against a live radar."""
import asyncio
import json
import sys
from contextlib import AsyncExitStack

from mcp import ClientSession
from mcp.client.streamable_http import streamable_http_client


async def main():
    url = sys.argv[1] if len(sys.argv) > 1 else 'http://192.168.18.136:83/mcp'
    uri = 'partner://sensors/presence'
    updates = [asyncio.Queue(), asyncio.Queue()]
    async with AsyncExitStack() as stack:
        sessions = []
        for index in range(2):
            read, write = await stack.enter_async_context(streamable_http_client(url))
            async def handler(message, index=index):
                data = message.model_dump(mode='json', by_alias=True)
                if data.get('method') == 'notifications/resources/updated':
                    updates[index].put_nowait(data)
            session = await stack.enter_async_context(ClientSession(read, write, message_handler=handler))
            await session.initialize()
            resources = await session.list_resources()
            assert {str(r.uri) for r in resources.resources} == {uri, 'partner://sensors/climate'}
            await session.subscribe_resource(uri)
            state = await session.read_resource(uri)
            print('client', index, 'initial', state.contents[0].text, flush=True)
            sessions.append(session)
        # A normal tool request must still work while both streams are open.
        assert not (await sessions[0].call_tool('info', {})).is_error
        print('both clients subscribed; waiting for radar change', flush=True)
        for index in range(2):
            event = await asyncio.wait_for(updates[index].get(), 90)
            assert event['params']['uri'] == uri, event
            state = await sessions[index].read_resource(uri)
            print('client', index, 'event', json.dumps(event), 'state', state.contents[0].text, flush=True)
        await sessions[0].unsubscribe_resource(uri)
        await sessions[1].unsubscribe_resource(uri)
        print('two-client event test passed', flush=True)


if __name__ == '__main__':
    asyncio.run(main())
