"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.activate = activate;
exports.deactivate = deactivate;
const vscode = require("vscode");
let goofyHighlightsEnabled = true;
function activate(context) {
    console.log('Lang.li extension is active!');
    // Register the toggle highlights command
    let toggleCmd = vscode.commands.registerCommand('langli.toggleGoofyHighlights', () => {
        goofyHighlightsEnabled = !goofyHighlightsEnabled;
        const config = vscode.workspace.getConfiguration('editor');
        if (!goofyHighlightsEnabled) {
            // Force a neutral color on source.langli
            config.update('tokenColorCustomizations', {
                "textMateRules": [
                    {
                        "scope": "source.langli",
                        "settings": {
                            "foreground": "#999999"
                        }
                    }
                ]
            }, vscode.ConfigurationTarget.Global).then(() => {
                vscode.window.showInformationMessage("Goofy highlights off");
            });
        }
        else {
            // Remove the override so normal colors appear
            config.update('tokenColorCustomizations', {}, vscode.ConfigurationTarget.Global)
                .then(() => {
                vscode.window.showInformationMessage("Goofy highlights on");
            });
        }
    });
    context.subscriptions.push(toggleCmd);
}
function deactivate() {
    // Optional cleanup logic
}
//# sourceMappingURL=extension.js.map