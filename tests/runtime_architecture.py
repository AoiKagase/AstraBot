"""Conservative lexical guards for the runtime's deliberately small declaration grammar.

This is not a C++ compiler. Unknown storage syntax fails closed so the inventory
cannot silently stop covering a newly introduced declaration form.
"""
from collections import namedtuple
from pathlib import PurePosixPath
import re

Token = namedtuple('Token', 'value kind line')


class ArchitectureError(AssertionError):
    pass


def lex(source):
    tokens = []
    cursor = 0
    line = 1
    pattern = re.compile(r'\s+|//[^\n]*|/\*[\s\S]*?\*/|(?:u8|u|U|L)?"(?:\\.|[^"\\])*"|'
                         r"(?:u|U|L)?'(?:\\.|[^'\\])*'|[A-Za-z_]\w*|\d+(?:\.\d+)?|"
                         r'::|->|&&|!=|==|<=|>=|\.\.\.|[^\s]')
    while cursor < len(source):
        raw = re.match(r'(?:u8|u|U|L)?R"([^ ()\\\t\r\n]{0,16})\(', source[cursor:])
        if raw:
            terminator = ')' + raw[1] + '"'
            end = source.find(terminator, cursor + raw.end())
            if end < 0:
                raise ArchitectureError('unterminated raw literal')
            value = source[cursor:end + len(terminator)]
            kind = 'literal'
        else:
            match = pattern.match(source, cursor)
            if not match:
                raise ArchitectureError('unsupported token at line ' + str(line))
            value = match[0]
            kind = 'literal' if re.match(r'(?:u8|u|U|L)?["\']', value) else 'code'
        if not value.isspace() and not value.startswith(('//', '/*')):
            tokens.append(Token(value, kind, line))
        line += value.count('\n')
        cursor += len(value)
    return tokens


def paired(tokens, start, opening='{', closing='}'):
    depth = 0
    for offset in range(start, len(tokens)):
        if tokens[offset].kind != 'code':
            continue
        depth += (tokens[offset].value == opening) - (tokens[offset].value == closing)
        if depth == 0:
            return offset
    raise ArchitectureError('unterminated ' + opening)


def _aliases(tokens):
    aliases = {}
    # Only this class's lexical scope participates. Method bodies, initializer
    # lambdas and nested types have independent alias environments.
    direct = []
    cursor = 0
    while cursor < len(tokens):
        if tokens[cursor].kind == 'code' and tokens[cursor].value == '{':
            cursor = paired(tokens, cursor) + 1
        else:
            direct.append(tokens[cursor])
            cursor += 1
    text = ' '.join(t.value if t.kind == 'code' else '<literal>' for t in direct)
    for name, target in re.findall(r'\busing (\w+) = ([^;]+);', text):
        aliases[name] = target.split()
    for target, name in re.findall(r'\btypedef ([^;]+?) (\w+) ;', text):
        aliases[name] = target.split()
    return aliases


def _expanded(words, aliases):
    for _ in range(len(aliases) + 1):
        for i, word in enumerate(words):
            if word in aliases and ('::' in words[max(0, i - 1):i] or '::' in words[i + 1:i + 2]):
                raise ArchitectureError('unsupported qualified type alias')
            if word in aliases and any(part in aliases[word] for part in ('*', '&', '&&')) and (
                    words[max(0, i - 1):i] in (['const'], ['volatile']) or
                    words[i + 1:i + 2] in (['const'], ['volatile'])):
                raise ArchitectureError('unsupported qualified indirection alias')
        expanded = [part for word in words for part in aliases.get(word, [word])]
        if expanded == words:
            return words
        words = expanded
    raise ArchitectureError('unsupported cyclic type alias')


def _readonly_return(words):
    # Qualifiers inside a template describe its arguments, not the returned object.
    outer = []
    depth = 0
    for word in words:
        if word == '<':
            depth += 1
        elif word == '>':
            depth -= 1
        elif depth == 0:
            outer.append(word)
    if depth != 0:
        raise ArchitectureError('unsupported return type')
    levels = [[]]
    reference = False
    for word in outer:
        if word == '*':
            levels.append([])
        elif word in ('&', '&&'):
            reference = True
        else:
            levels[-1].append(word)
    # A returned pointer value is a copy. Every object reachable through it,
    # including intermediate pointers, must be const; a reference also exposes
    # the outermost pointer object itself.
    exposed = levels if reference else levels[:-1]
    return all('const' in level for level in exposed)


def _members(tokens, scope, aliases, default_access='private', owner_name='ActorState'):
    result = set()
    access = default_access
    cursor = 0
    while cursor < len(tokens):
        token = tokens[cursor]
        if token.value == '#':
            cursor += 1
            while cursor < len(tokens) and tokens[cursor].line == token.line:
                cursor += 1
            continue
        if token.value == ';':
            cursor += 1
            continue
        if token.value in ('public', 'protected', 'private') and tokens[cursor + 1].value == ':':
            access = token.value
            cursor += 2
            continue
        start = cursor
        angle = paren = bracket = 0
        while cursor < len(tokens):
            word = tokens[cursor].value
            if tokens[cursor].kind == 'code':
                if word in ('{', ';') and paren == bracket == 0:
                    break
                angle += (word == '<') - (word == '>')
                paren += (word == '(') - (word == ')')
                bracket += (word == '[') - (word == ']')
            cursor += 1
        if cursor == len(tokens):
            raise ArchitectureError('unsupported unterminated declaration')
        words = [t.value if t.kind == 'code' else '<literal>' for t in tokens[start:cursor]]
        if not words:
            raise ArchitectureError('unsupported empty declaration')
        nested = words[0] in ('class', 'struct', 'enum')
        if nested:
            if tokens[cursor].value == '{':
                end = paired(tokens, cursor)
                if words[:2] == ['struct', 'ActorState']:
                    if access != 'private':
                        raise ArchitectureError('writable ActorState type is not private')
                    body = tokens[cursor + 1:end]
                    result |= _members(body, 'ActorState', dict(aliases, **_aliases(body)), 'public')
                cursor = end + 1
                if cursor >= len(tokens) or tokens[cursor].value != ';':
                    raise ArchitectureError('unsupported nested type instance declaration')
            cursor += 1
            continue
        if words[0] in ('using', 'typedef') or 'friend' in words:
            if 'friend' in words:
                allowed = {
                    ('friend', 'const', 'char', '*', 'HookCommandArgs', '(', ')'),
                    ('friend', 'const', 'char', '*', 'HookCommandArgv', '(', 'int', 'index', ')'),
                    ('friend', 'int', 'HookCommandArgc', '(', ')'),
                }
                if (owner_name != 'PluginRuntime' or scope != 'owner' or
                        tuple(words) not in allowed or tokens[cursor].value != ';'):
                    raise ArchitectureError('writable ownership through unapproved friend')
            cursor += 1
            continue
        if 'template' in words:
            raise ArchitectureError('unsupported template declaration scope')
        equals = words.index('=') if '=' in words else len(words)
        declaration = words[:equals]
        angle_depth = 0
        opening = None
        for offset, word in enumerate(declaration):
            angle_depth += (word == '<') - (word == '>')
            if word == '(' and angle_depth == 0:
                opening = offset
                break
        is_method = opening is not None
        if is_method:
            prefix = declaration[:opening]
            function_prefix = [word for word in prefix if word not in ('static', 'inline', 'virtual', 'constexpr')]
            type_keywords = {'void', 'bool', 'char', 'char16_t', 'char32_t', 'wchar_t', 'short', 'int',
                             'long', 'signed', 'unsigned', 'float', 'double', 'const', 'volatile', 'auto', 'decltype'}
            return_prefix = [word for word in function_prefix[:-1]
                             if word not in ('const', 'volatile', 'class', 'struct', 'enum', 'typename')]
            if (declaration[opening + 1:opening + 2] in (['*'], ['&'], ['&&']) or
                    'thread_local' in prefix or
                    prefix[-1:] in (['*'], ['&'], ['&&']) or
                    ('static' in prefix and (not return_prefix or function_prefix[-2:-1] == ['::'] or
                                             function_prefix[-1] in type_keywords or
                                             not re.fullmatch(r'[A-Za-z_]\w*', function_prefix[-1])))):
                raise ArchitectureError('unsupported parenthesized storage declaration')
            if len(prefix) < 2:
                # Constructors/destructors have an explicit body or recognized owner name.
                if prefix != [owner_name]:
                    raise ArchitectureError('unsupported macro-like declaration')
            # Inspect the actual return type after the parameter list as well.
            closing = opening
            depth = 0
            for closing in range(opening, len(declaration)):
                depth += (declaration[closing] == '(') - (declaration[closing] == ')')
                if depth == 0:
                    break
            tail = declaration[closing + 1:]
            if '->' in tail:
                result_type = tail[tail.index('->') + 1:]
                result_type = [word for word in result_type if word not in ('override', 'final')]
                if not result_type or any(word in result_type for word in ('(', ')', '[', ']')):
                    raise ArchitectureError('unsupported trailing return type')
            else:
                result_type = prefix[:-1]
            return_type = _expanded(result_type, aliases)
            if access == 'public' and 'auto' in return_type:
                raise ArchitectureError('unsupported deduced public return type')
            if access == 'public' and any(x in return_type for x in ('&', '&&', '*')):
                singleton = prefix[-1] == 'instance' and return_type == ['static', owner_name, '&']
                if not singleton and not _readonly_return(return_type):
                    raise ArchitectureError('writable state exposure')
            if tokens[cursor].value == '{':
                cursor = paired(tokens, cursor) + 1
            else:
                cursor += 1
            continue
        if 'static' in declaration and 'constexpr' in declaration:
            if tokens[cursor].value == '{':
                cursor = paired(tokens, cursor) + 1
            if cursor < len(tokens) and tokens[cursor].value == ';':
                cursor += 1
            continue
        # A data member is one typed declarator, optionally followed by a value initializer.
        level = 0
        for word in declaration:
            level += (word == '<') - (word == '>')
            if word == ',' and level == 0:
                raise ArchitectureError('unsupported multiple storage declarators')
        if len(declaration) < 2 or not re.fullmatch(r'[A-Za-z_]\w*', declaration[-1]):
            raise ArchitectureError('unsupported storage declaration: ' + ' '.join(words))
        if scope == 'owner' and access != 'private':
            raise ArchitectureError('writable owner storage is not private')
        result.add(('static_' + scope if 'static' in declaration else scope, declaration[-1]))
        if tokens[cursor].value == '{':
            cursor = paired(tokens, cursor) + 1
            if cursor >= len(tokens) or tokens[cursor].value != ';':
                raise ArchitectureError('unsupported storage initializer')
        cursor += 1
    return result


def collect_members(source, owner):
    tokens = lex(source)
    for offset in range(len(tokens) - 1):
        if tokens[offset].value == 'class' and tokens[offset + 1].value == owner:
            start = next(i for i in range(offset + 2, len(tokens)) if tokens[i].value == '{')
            end = paired(tokens, start)
            body = tokens[start + 1:end]
            return _members(body, 'owner', _aliases(body), owner_name=owner)
    raise ArchitectureError('missing owner ' + owner)


def check_inventory(actual, expected):
    found = {(owner, scope, member) for owner, members in actual.items() for scope, member in members}
    if found != expected:
        raise ArchitectureError('ownership inventory mismatch; missing=' + repr(sorted(expected - found))
                                + '; unregistered=' + repr(sorted(found - expected)))


def check_dependencies(path, source):
    path = str(PurePosixPath(path.replace('\\', '/')))
    tokens = lex(source)
    code = ' '.join(t.value if t.kind == 'code' else '<literal>' for t in tokens)
    if path.endswith('/bot_behavior_buying.cpp') and re.search(r'\bupdateBehaviorPriorities\b', code):
        raise ArchitectureError('buying owns general behavior priority arbitration')
    if path.endswith(('/action_adapter.cpp', '/action_adapter.hpp')) and re.search(
            r'\b(?:PlantAttemptState|PlantAttemptController|evaluatePlantAttempt|kPlantAttemptTimeoutSeconds)\b', code):
        raise ArchitectureError('adapter owns plant policy')
    includes = []
    for i, token in enumerate(tokens[:-2]):
        if token.value == '#' and tokens[i + 1].value == 'include':
            end = i + 2
            parts = []
            while end < len(tokens) and tokens[end].line == token.line:
                parts.append(tokens[end].value)
                end += 1
            includes.append(''.join(parts).strip('"<>'))
    owner = re.search(r'/bot_(observation|behavior|navigation|command)_', path)
    if owner:
        name = owner[1]
        if name == 'observation' and re.search(
                r'\b(?:ReactionQueue|behavior_observeRecognizedTargetChange)\b', code):
            raise ArchitectureError('observation owns or mutates threat recognition')
        if re.search(r'\bconst_cast\b', code):
            raise ArchitectureError('writable ownership bypass through const_cast')
        for include in includes:
            peer = re.search(r'bot_(observation|behavior|navigation|command)_runtime\.hpp$', include)
            if include.endswith('plugin_runtime.hpp') or (peer and peer[1] != name):
                raise ArchitectureError('forbidden peer include: ' + include)
        if re.search(r'\bservices_ -> (?:observation|behavior|navigation|command) \(', code):
            raise ArchitectureError('forbidden peer owner accessor')
        if name != 'command' and re.search(r'\bservice_inputDispatcher \(', code):
            raise ArchitectureError('forbidden dispatcher acquisition')
        if name == 'command' and re.search(r'\bdecideManagedBotAction \(', code):
            raise ArchitectureError('command performs behavior decision')
        if name == 'navigation' and re.search(r'\b(?:executeManagedBotCommand|prepareNeutralManagedBotCommand) \(', code):
            raise ArchitectureError('navigation performs command orchestration')
    command = path.endswith('/bot_command_runtime.cpp')
    dispatcher = path.endswith('/input_dispatcher.cpp')
    if not command and not dispatcher:
        for i, token in enumerate(tokens):
            if token.kind == 'code' and token.value == 'pfnRunPlayerMove':
                suffix = [t.value for t in tokens[i + 1:i + 3]]
                if suffix not in (['!=', 'nullptr'], ['==', 'nullptr']):
                    raise ArchitectureError('forbidden input dispatch capability acquisition or call')
        values = [t.value if t.kind == 'code' else '<literal>' for t in tokens]
        root_cpp = path == 'src/adapter/metamod/plugin_runtime.cpp'
        root_header = path == 'src/adapter/metamod/plugin_runtime.hpp'
        for i, value in enumerate(values):
            if value == 'service_inputDispatcher':
                signature = values[i - 2:i] == ['Service_inputDispatcher', '&']
                definition = (root_header and signature and
                              values[i + 1:i + 6] == ['(', ')', 'override', '{', 'return'])
                interface = (path == 'src/adapter/metamod/bot_runtime_services.hpp' and signature and
                             values[i + 1:i + 6] == ['(', ')', '=', '0', ';'])
                if not (definition or interface):
                    raise ArchitectureError('forbidden input dispatcher getter acquisition')
            if value != 'inputDispatcher_':
                continue
            after = values[i + 1:]
            before = values[:i]
            management = (root_cpp and len(after) >= 3 and after[0] == '.'
                          and after[1] in ('configure', 'reset', 'bindActor', 'unbindActor')
                          and after[2] == '(')
            construction = (root_cpp and after[:1] == ['('] and before[-1:] == [','])
            declaration = (root_header and before[-1:] == ['InputDispatcher'] and after[:1] == [';'])
            command_port = (root_header and before[-8:] ==
                            ['Service_inputDispatcher', '&', 'service_inputDispatcher', '(', ')', 'override', '{', 'return']
                            and after[:2] == [';', '}'])
            if not (management or construction or declaration or command_port):
                raise ArchitectureError('forbidden input dispatcher escape or dispatch call')
    if path.startswith(('src/core/', 'include/astrabot/')) and '/metamod/' not in path:
        sdk_headers = {'eiface.h', 'extdll.h', 'edict.h', 'entvars.h', 'usercmd.h', 'meta_api.h',
                       'regamedll_api.h', 'dllapi.h', 'enginecallback.h'}
        for include in includes:
            if PurePosixPath(include).name in sdk_headers or any(x in include for x in ('metamod/', 'hlsdk/', 'cssdk/')):
                raise ArchitectureError('Core depends on SDK: ' + include)
