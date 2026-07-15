# Project Integration Tests

Editor-only automation harness for cross-plugin integration scenarios. Standalone `UnrealEditor -game` runners explicitly preload the editor-built module before dispatching automation, while Game, Client, Server, and Shipping target graphs remain unaffected. The plugin owns long-running PIE and standalone-game flows and provides a home for future simulated end-to-end checks. Additional modules can be added for specialized domains (for example networking or saves) without impacting packaged runtime builds.
