"""Negative cases for the source ownership and dependency gate."""
import unittest

from runtime_architecture import ArchitectureError, check_dependencies, collect_members, check_inventory
from runtime_movement_integration_contract import assert_neutral_preparation_order


class ArchitectureCheckerTests(unittest.TestCase):
    def test_observation_cannot_own_or_mutate_threat_recognition(self):
        for source in ('compat::ReactionQueue reactionQueues;',
                       'services_->behavior_observeRecognizedTargetChange(index, now);'):
            with self.subTest(source=source), self.assertRaisesRegex(ArchitectureError, 'recognition'):
                check_dependencies('src/adapter/metamod/bot_observation_runtime.cpp', source)

    def test_observation_recognition_guard_ignores_documentation(self):
        check_dependencies('src/adapter/metamod/bot_observation_runtime.hpp',
                           '// compat::ReactionQueue\nconst char *text = "behavior_observeRecognizedTargetChange";')

    def test_const_reference_alias_does_not_make_referent_const(self):
        for alias in ('using Ref = int &;', 'using Ref = int &&;', 'using Ref = int *;',
                      'using Base = int &; using Ref = Base;', 'typedef int & Ref;'):
            for signature in ('auto state() -> const Ref', 'Ref const state()',
                              'auto state() -> const volatile Ref', 'Ref volatile const state()'):
                with self.subTest(alias=alias, signature=signature), self.assertRaisesRegex(ArchitectureError, 'unsupported'):
                    collect_members('class Owner { ' + alias + ' public: ' + signature + '; };', 'Owner')

    def test_undeduced_public_return_cannot_hide_pointer_exposure(self):
        for result in ('auto', 'const auto'):
            with self.subTest(result=result), self.assertRaisesRegex(ArchitectureError, 'unsupported'):
                collect_members('class Owner { private: int value_; public: ' + result +
                                ' state() { return &value_; } };', 'Owner')
        with self.assertRaisesRegex(ArchitectureError, 'unsupported'):
            collect_members('class Owner { private: int value_; public: '
                            'auto state() -> auto { return &value_; } };', 'Owner')

    def test_parenthesized_storage_is_not_mistaken_for_a_method(self):
        for declaration in ('static int (hidden_);', 'static int (*hidden_);',
                            'static Widget (hidden_);', 'static Widget * (hidden_);',
                            'static unsigned int (hidden_);', 'static volatile int (hidden_);',
                            'static const Widget (hidden_);', 'static std::array<int, 2> (hidden_);',
                            'static struct Widget (hidden_);', 'static thread_local Widget (hidden_);'):
            with self.subTest(declaration=declaration), self.assertRaisesRegex(ArchitectureError, 'unsupported'):
                collect_members('class Owner { private: ' + declaration + ' int value_; };', 'Owner')

    def test_static_factories_are_still_methods(self):
        source = ('class Owner { public: static unsigned int count(); static const Widget make(); '
                  'static struct Widget create(); static std::array<int, 2> values(); };')
        self.assertEqual(collect_members(source, 'Owner'), set())

    def test_prefixed_friend_declarations_are_rejected(self):
        for declaration in ('inline friend void mutate(Owner &);',
                            'template<class T> friend void mutate(Owner &, T);'):
            with self.subTest(declaration=declaration), self.assertRaisesRegex(ArchitectureError, 'friend'):
                collect_members('class Owner { private: int value_; ' + declaration + ' };', 'Owner')

    def test_qualified_alias_cannot_use_unrelated_outer_alias(self):
        with self.assertRaisesRegex(ArchitectureError, 'unsupported'):
            collect_members('class Owner { using View = const int; struct Context { using View = int; }; '
                            'public: auto state() -> Context::View &; };', 'Owner')

    def test_template_parameter_cannot_use_shadowed_class_alias(self):
        for signature in ('auto state() -> View &', 'View &state()'):
            with self.subTest(signature=signature), self.assertRaisesRegex(ArchitectureError, 'unsupported'):
                collect_members('class Owner { using View = const int; public: '
                                'template<typename View> ' + signature + '; };', 'Owner')

    def test_static_mutable_storage_must_be_registered(self):
        for declaration in ('static int hidden_;', 'static thread_local int hidden_;'):
            with self.subTest(declaration=declaration):
                members = collect_members('class Owner { private: ' + declaration + ' int value_; };', 'Owner')
                self.assertIn(('static_owner', 'hidden_'), members)
                with self.assertRaisesRegex(ArchitectureError, 'inventory'):
                    check_inventory({'Owner': members}, {('Owner', 'owner', 'value_')})

    def test_static_constants_and_methods_are_not_mutable_storage(self):
        self.assertEqual(collect_members('class Owner { private: static constexpr int limit = 2; '
                                         'static void reset(); int value_; };', 'Owner'), {('owner', 'value_')})

    def test_trailing_returns_cannot_expose_writable_state(self):
        for result in ('int &', 'ActorState *', 'const int **', 'std::pair<const int, int> &'):
            with self.subTest(result=result), self.assertRaisesRegex(ArchitectureError, 'writable'):
                collect_members('class Owner { private: int value_; public: auto state() -> '
                                + result + ' { return value_; } };', 'Owner')

    def test_readonly_trailing_return_alias_is_allowed(self):
        self.assertEqual(collect_members('class Owner { using View = const int; public: '
                                         'auto state() const noexcept -> View &; };', 'Owner'), set())

    def test_method_local_alias_cannot_override_class_alias(self):
        for local in ('using View = const int;', 'typedef const int View;'):
            source = ('class Owner { private: struct ActorState { int value; }; using View = ActorState; '
                      'void helper() { ' + local + ' } public: View &state(); };')
            with self.subTest(local=local), self.assertRaisesRegex(ArchitectureError, 'writable'):
                collect_members(source, 'Owner')

    def test_nested_type_alias_cannot_override_class_alias(self):
        with self.assertRaisesRegex(ArchitectureError, 'writable'):
            collect_members('class Owner { using View = int; struct Context { using View = const int; }; '
                            'public: View &state(); };', 'Owner')

    def test_actor_alias_uses_its_own_scope(self):
        self.assertEqual(collect_members('class Owner { using View = int; private: struct ActorState { '
                                         'using View = const int; View &read(); int value; }; };', 'Owner'),
                         {('ActorState', 'value')})

    def test_unapproved_friend_functions_are_rejected(self):
        for owner, declaration in (('Owner', 'friend void mutate(Owner &);'),
                                   ('Owner', 'friend struct Writer;'),
                                   ('PluginRuntime', 'friend int HookCommandArgc(int extra);'),
                                   ('PluginRuntime', 'friend int HookCommandArgc() { return 0; }')):
            with self.subTest(declaration=declaration), self.assertRaisesRegex(ArchitectureError, 'friend'):
                collect_members('class ' + owner + ' { private: int value_; ' + declaration + ' };', owner)

    def test_only_existing_root_hook_friend_declarations_are_allowed(self):
        source = ('class PluginRuntime { private: friend const char *HookCommandArgs(); '
                  'friend const char *HookCommandArgv(int index); friend int HookCommandArgc(); int value_; };')
        self.assertEqual(collect_members(source, 'PluginRuntime'), {('owner', 'value_')})

    def test_template_const_does_not_qualify_returned_container(self):
        for signature in ('std::pair<const int, int> &state()',
                          'std::pair<const int, int> &&state()',
                          'std::pair<const int, int> *state()'):
            with self.subTest(signature=signature), self.assertRaisesRegex(ArchitectureError, 'writable'):
                collect_members('class Owner { public: ' + signature + '; };', 'Owner')

    def test_each_indirect_return_level_must_be_readonly(self):
        for signature in ('const int **state()', 'const int *const *&state()',
                          'int *const *state()', 'const int **const state()'):
            with self.subTest(signature=signature), self.assertRaisesRegex(ArchitectureError, 'writable'):
                collect_members('class Owner { public: ' + signature + '; };', 'Owner')

    def test_readonly_return_levels_remain_allowed(self):
        for signature in ('const std::pair<const int, int> &state()',
                          'std::pair<const int, int> const &state()',
                          'const int *state()', 'const int *const *state()',
                          'const int *const &state()', 'const int *const *const &state()'):
            with self.subTest(signature=signature):
                self.assertEqual(collect_members('class Owner { public: ' + signature + '; };', 'Owner'), set())

    def test_root_dispatcher_cannot_escape_through_alias_or_argument(self):
        for source in ('auto &out = inputDispatcher_; out.dispatchNext(actor, frame);',
                       'auto *out = &inputDispatcher_; out->enqueue(command);',
                       'auto &out = this->inputDispatcher_; auto &other = out; other.dispatchNext(actor, frame);',
                       'send(inputDispatcher_);', 'return inputDispatcher_;',
                       'auto send = &InputDispatcher::dispatchNext; (inputDispatcher_.*send)(actor, frame);'):
            with self.subTest(source=source), self.assertRaisesRegex(ArchitectureError, 'dispatch'):
                check_dependencies('src/adapter/metamod/plugin_runtime.cpp', source)

    def test_root_dispatcher_management_remains_allowed(self):
        check_dependencies('src/adapter/metamod/plugin_runtime.cpp',
                           'inputDispatcher_.configure(engine, profiler); inputDispatcher_.reset(); '
                           'inputDispatcher_.bindActor(actor, entity); inputDispatcher_.unbindActor(actor);')

    def test_root_cannot_acquire_command_port_through_its_getter(self):
        for source in ('auto &out = service_inputDispatcher(); out.dispatchNext(actor, frame);',
                       'auto *out = &this->service_inputDispatcher();',
                       'auto get = &PluginRuntime::service_inputDispatcher;'):
            with self.subTest(source=source), self.assertRaisesRegex(ArchitectureError, 'dispatch'):
                check_dependencies('src/adapter/metamod/plugin_runtime.cpp', source)

    def test_command_port_is_the_only_root_dispatcher_return(self):
        path = 'src/adapter/metamod/plugin_runtime.hpp'
        check_dependencies(path, 'InputDispatcher inputDispatcher_; '
                           'Service_inputDispatcher &service_inputDispatcher() override { return inputDispatcher_; }')
        with self.assertRaisesRegex(ArchitectureError, 'dispatch'):
            check_dependencies(path, 'Service_inputDispatcher &other() { return inputDispatcher_; }')

    def test_qualified_pointer_alias_is_not_mistaken_for_const_pointee(self):
        for signature in ('const Pointer state()', 'Pointer const state()'):
            with self.subTest(signature=signature), self.assertRaisesRegex(ArchitectureError, 'unsupported'):
                collect_members('class Owner { using Pointer = int *; public: ' + signature + '; };', 'Owner')

    def test_readonly_alias_returns_remain_allowed(self):
        source = ('class Owner { using View = const std::pair<const int, int>; '
                  'using ReadPointer = const int *; public: View &view(); ReadPointer pointer(); };')
        self.assertEqual(collect_members(source, 'Owner'), set())

    def test_root_cannot_reset_preparation_before_recording_dispatch(self):
        good = ('prepareNeutralManagedBotCommand(); command_.executeManagedBotCommand(); '
                'navigation_.recordMovementPhysicsSample(); navigation_.completePreparation();')
        assert_neutral_preparation_order(good)
        bad = ('prepareNeutralManagedBotCommand(); command_.executeManagedBotCommand(); '
               'navigation_.completePreparation(); navigation_.recordMovementPhysicsSample();')
        with self.assertRaisesRegex(AssertionError, 'complete after'):
            assert_neutral_preparation_order(bad)
    def test_new_member_cannot_be_omitted_from_inventory(self):
        members = collect_members('class Owner { private: int old_; int added_; };', 'Owner')
        with self.assertRaisesRegex(ArchitectureError, 'inventory'):
            check_inventory({'Owner': members}, {('Owner', 'owner', 'old_')})

    def test_wrong_owner_is_not_accepted(self):
        with self.assertRaisesRegex(ArchitectureError, 'inventory'):
            check_inventory({'B': {('owner', 'field_')}}, {('A', 'owner', 'field_')})

    def test_actor_alias_cannot_expose_writable_state(self):
        for alias in ('using View = ActorState;', 'typedef ActorState View;'):
            with self.subTest(alias=alias), self.assertRaisesRegex(ArchitectureError, 'writable'):
                collect_members('class Owner { private: struct ActorState { int value; }; '
                                + alias + ' public: View &state(); };', 'Owner')

    def test_primitive_reference_cannot_expose_writable_state(self):
        with self.assertRaisesRegex(ArchitectureError, 'writable'):
            collect_members('class Owner { public: int &value(); private: int value_; };', 'Owner')

    def test_static_and_top_level_const_do_not_hide_writable_returns(self):
        for signature in ('static int &state()', 'int * const state()', 'const int *&state()'):
            with self.subTest(signature=signature), self.assertRaisesRegex(ArchitectureError, 'writable'):
                collect_members('class Owner { public: ' + signature + '; private: int state_; };', 'Owner')

    def test_function_pointer_alias_cannot_bypass_dispatch_boundary(self):
        with self.assertRaisesRegex(ArchitectureError, 'dispatch'):
            check_dependencies('src/adapter/metamod/bot_navigation_runtime.cpp',
                               'auto run = engine->pfnRunPlayerMove; run(entity, angles);')

    def test_private_actor_storage_is_counted(self):
        source = 'class Owner { private: struct ActorState { int value{}; }; ActorState actor_{}; };'
        self.assertEqual(collect_members(source, 'Owner'), {('ActorState', 'value'), ('owner', 'actor_')})

    def test_template_expression_is_not_mistaken_for_a_method(self):
        source = 'class Owner { private: std::array<bool, static_cast<unsigned>(Kind::Count)> flags_{}; };'
        self.assertEqual(collect_members(source, 'Owner'), {('owner', 'flags_')})

    def test_comments_literals_methods_and_context_types_are_not_storage(self):
        source = '''class Owner { private:
            // int fake_;
            struct Context { int temporary; };
            static constexpr int constant = 1;
            void reset() { int temporary; const char *s = "int fake_;"; }
            const char *text_ = R"tag(int fake_; { } //)tag";
            int real_{};
        };'''
        self.assertEqual(collect_members(source, 'Owner'), {('owner', 'text_'), ('owner', 'real_')})

    def test_unrecognized_declaration_fails_closed(self):
        with self.assertRaisesRegex(ArchitectureError, 'unsupported'):
            collect_members('class Owner { private: DECLARE_STATE(hidden_); };', 'Owner')

    def test_function_pointer_storage_fails_closed(self):
        with self.assertRaisesRegex(ArchitectureError, 'unsupported'):
            collect_members('class Owner { private: void (*callback_)(int); int old_; };', 'Owner')

    def test_dispatch_outside_command_is_forbidden(self):
        with self.assertRaisesRegex(ArchitectureError, 'dispatch'):
            check_dependencies('src/adapter/metamod/bot_navigation_runtime.cpp',
                               'engine->pfnRunPlayerMove(entity, angles);')

    def test_dispatcher_alias_acquisition_is_forbidden(self):
        with self.assertRaisesRegex(ArchitectureError, 'dispatcher'):
            check_dependencies('src/adapter/metamod/bot_behavior_combat.cpp',
                               'auto &dispatcher = services_->service_inputDispatcher();')

    def test_availability_check_is_allowed(self):
        check_dependencies('src/adapter/metamod/bot_navigation_runtime.cpp',
                           'bool available = engine->pfnRunPlayerMove != nullptr;')

    def test_root_bind_allowed_but_root_dispatch_is_not(self):
        check_dependencies('src/adapter/metamod/plugin_runtime.cpp', 'inputDispatcher_.bindActor(actor, entity);')
        with self.assertRaisesRegex(ArchitectureError, 'dispatch'):
            check_dependencies('src/adapter/metamod/plugin_runtime.cpp', 'inputDispatcher_.dispatchNext(actor, frame);')

    def test_fake_dependency_code_in_comments_and_strings_is_ignored(self):
        check_dependencies('src/adapter/metamod/bot_behavior_combat.cpp', '''
            // #include "plugin_runtime.hpp"
            const char *sample = "services_->command().enqueue(command);";
            const char *raw = R"(engine->pfnRunPlayerMove(entity);)";
        ''')

    def test_split_owner_cannot_include_peer(self):
        with self.assertRaisesRegex(ArchitectureError, 'peer'):
            check_dependencies('src/adapter/metamod/bot_behavior_combat.cpp',
                               '#include "bot_command_runtime.hpp"')

    def test_core_cannot_include_sdk(self):
        with self.assertRaisesRegex(ArchitectureError, 'SDK'):
            check_dependencies('src/core/runtime/example.cpp', '#include "eiface.h"')


if __name__ == '__main__':
    unittest.main()
