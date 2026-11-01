#!/usr/bin/env python3
"""Test reminder migration and MCP operations against a live Partner."""
import asyncio
import json
import sys
from mcp import ClientSession
from mcp.client.streamable_http import streamable_http_client


async def main():
    url = sys.argv[1] if len(sys.argv) > 1 else 'http://192.168.18.136:83/mcp'
    with open('/tmp/partner-reminder-migration-marker.json') as f:
        marker = json.load(f)
    with open('/tmp/partner-reminders-before.json') as f:
        before = json.load(f)
    created = []
    async with streamable_http_client(url) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            async def call(name, args=None):
                result = await session.call_tool(name, args or {})
                if result.is_error: raise AssertionError((name, result.content))
                return json.loads(result.content[0].text)
            async def invalid(name, args):
                result = await session.call_tool(name, args)
                assert result.is_error, (name, args, result)
            listing = (await call('reminder_list'))['reminders']
            comparable = lambda r: (r['id'], r['due_epoch'], r['text'])
            assert sorted(map(comparable, listing)) == sorted(map(comparable, before + [marker]))
            print('existing reminders and pre-upgrade marker survived reboot', flush=True)
            try:
                for args in ({'text':'Uji migrasi MCP', 'after_seconds':86400},
                             {'text':'Uji epoch MCP', 'due_epoch':marker['due_epoch'] + 1}):
                    item = await call('reminder_create', args)
                    created.append(item['id'])
                    assert any(r['id'] == item['id'] and r['text'] == item['text'] for r in (await call('reminder_list'))['reminders'])
                await invalid('reminder_create', {'text':'x', 'after_seconds':-1})
                await invalid('reminder_create', {'text':'x', 'after_seconds':1, 'due_epoch':marker['due_epoch']})
                await invalid('reminder_create', {'text':'🙂'*41, 'after_seconds':10})
                await invalid('reminder_create', {'text':'abc\0def', 'after_seconds':10})
                await invalid('reminder_delete', {'id':0})
                print('create, list and invalid input checks passed', flush=True)
            finally:
                for item_id in created + [marker['id']]:
                    await call('reminder_delete', {'id':item_id})
            remaining = (await call('reminder_list'))['reminders']
            assert sorted(map(comparable, remaining)) == sorted(map(comparable, before))
            print('test reminders removed; original data preserved', flush=True)


if __name__ == '__main__':
    asyncio.run(main())
