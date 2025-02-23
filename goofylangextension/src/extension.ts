import * as vscode from 'vscode';

let goofyHighlightsEnabled = true;

export function activate(context: vscode.ExtensionContext) {
    console.log('Lang.li extension is active!');

    // Register the toggle highlights command
    let toggleCmd = vscode.commands.registerCommand('langli.toggleGoofyHighlights', () => {
        goofyHighlightsEnabled = !goofyHighlightsEnabled;

        const config = vscode.workspace.getConfiguration('editor');
        if (!goofyHighlightsEnabled) {
            // Force a neutral color on source.langli
            config.update(
                'tokenColorCustomizations',
                {
                    "textMateRules": [
                        {
                            "scope": "source.langli",
                            "settings": {
                                "foreground": "#999999"
                            }
                        }
                    ]
                },
                vscode.ConfigurationTarget.Global
            ).then(() => {
                vscode.window.showInformationMessage("Goofy highlights off");
            });
        } else {
            // Remove the override so normal colors appear
            config.update('tokenColorCustomizations', {}, vscode.ConfigurationTarget.Global)
            .then(() => {
                vscode.window.showInformationMessage("Goofy highlights on");
            });
        }
    });

    context.subscriptions.push(toggleCmd);
}

export function deactivate() {
    // Optional cleanup logic
}
