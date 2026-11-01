#!/usr/bin/env python3
"""Exercise Partner with the official MCP Python SDK; expects an active device."""
import asyncio
import json
import sys
from mcp import ClientSession
from mcp.client.streamable_http import streamable_http_client


async def main():
    url = sys.argv[1] if len(sys.argv) > 1 else 'http://192.168.18.136:83/mcp'
    async with streamable_http_client(url) as (read, write):
        async with ClientSession(read, write) as session:
            initialized = await session.initialize()
            print('initialized', initialized.protocol_version, initialized.server_info.name, flush=True)
            tools = await session.list_tools()
            names = {item.name for item in tools.tools}
            assert names == {'contract', 'menu', 'menu_list', 'info', 'speak', 'expression', 'expression_list', 'logs', 'reminder_list', 'reminder_create', 'reminder_delete'}, names
            assert next(item for item in tools.tools if item.name == 'speak').input_schema['properties']['text']['maxLength'] == 255
            print('tools', sorted(names), flush=True)

            async def call(name, args=None):
                result = await session.call_tool(name, args or {})
                if result.is_error:
                    raise AssertionError(f'{name}: {result.content}')
                assert result.content and result.content[0].type == 'text'
                return result.content[0].text

            core = await call('contract')
            assert 'Partner operating contract' in core
            assert 'Screens and expressions' in await call('contract', {'reference': 'screens'})
            missing = await session.call_tool('contract', {'reference': 'missing'})
            assert missing.is_error
            too_long = await session.call_tool('speak', {'text': 'a' * 256})
            assert too_long.is_error and '255' in too_long.content[0].text
            too_many_emoji = await session.call_tool('speak', {'text': '🙂' * 256})
            assert too_many_emoji.is_error and '255' in too_many_emoji.content[0].text
            embedded_nul = await session.call_tool('speak', {'text': 'hello\0world'})
            assert embedded_nul.is_error
            menus = json.loads(await call('menu_list'))
            expressions = json.loads(await call('expression_list'))
            assert menus['screens'] == ['home', 'clock', 'sensors']
            assert 'happy' in expressions['expressions'] and 'auto' in expressions['expressions']
            before = json.loads(await call('info'))
            print('info', json.dumps(before, ensure_ascii=False), flush=True)
            old_menu, old_expression = before['screen'], before['expression']
            try:
                await call('menu', {'screen': 'clock'})
                assert json.loads(await call('info'))['screen'] == 'clock'
                await call('menu', {'screen': 'sensors'})
                assert json.loads(await call('info'))['screen'] == 'sensors'
                await call('menu', {'screen': 'home'})
                await call('expression', {'choose': 'happy'})
                assert json.loads(await call('info'))['expression'] == 'happy'
                bad = await session.call_tool('expression', {'choose': 'missing'})
                assert bad.is_error
                queued = json.loads(await call('speak', {'text': 'Halo, ini uji MCP Partner.'}))
                assert queued['status'] == 'queued'
                print('speech queued', flush=True)
                await asyncio.sleep(12)
                for _ in range(30):
                    logs = await call('logs')
                    logs = logs.rsplit('[mcp] speech queued: Halo, ini uji MCP Partner.', 1)[-1]
                    if '[mcp] speech complete' in logs:
                        print('speech complete', flush=True)
                        break
                    if '[mcp] speech failed' in logs:
                        raise AssertionError('speech failed: '+logs[-1200:])
                    await asyncio.sleep(1)
                else:
                    raise AssertionError('speech did not complete in 30 seconds')
            finally:
                await call('menu', {'screen': old_menu})
                await call('expression', {'choose': old_expression})
            print('MCP SDK live test passed', flush=True)


if __name__ == '__main__':
    asyncio.run(main())
