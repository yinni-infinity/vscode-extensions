// The module 'vscode' contains the VS Code extensibility API
// Import the module and reference it with the alias vscode in your code below
import * as vscode from 'vscode';
import path from 'path';
import fs from 'fs';
import os from 'os';

function goToPositionInFile(filePath: string, lineNumber: number): void {
	vscode.workspace.openTextDocument(filePath).then(document => {
		vscode.window.showTextDocument(document).then(editor => {
			const position = new vscode.Position(lineNumber - 1, 0);
			editor.selection = new vscode.Selection(position, position);
			editor.revealRange(new vscode.Range(position, position), vscode.TextEditorRevealType.InCenter);
		}, error => {
			vscode.window.showErrorMessage(`Cannot open file: ${error}`);
		});
	}, error => {
		vscode.window.showErrorMessage(`Cannot open document: ${error}`);
	});
}

async function promptForDirectoryPath() {
	const userInput = await vscode.window.showInputBox({
		placeHolder: '请输入coverity检查结果所在的目录（是目录而不是文件）',
	});

	if (!userInput) {
		// 用户没有输入任何信息，可以选择什么都不做，或者提示用户输入信息
		return;
	}

	// 将路径中的~替换为用户的实际home目录路径
	const expandedPath = userInput.replace(/^~/, os.homedir());


	if (!fs.existsSync(expandedPath) || !fs.lstatSync(expandedPath).isDirectory()) {
		// 用户输入的路径不存在或者不是一个目录，需要提示用户重新输入
		vscode.window.showErrorMessage('Specified path is not a valid directory');
		return await promptForDirectoryPath();
	}




	// 用户输入的路径合法，可以继续使用该路径
	return expandedPath;
}

function createAndWriteFile(directory: string, filename: string, content: string) {
	const filePath = path.join(directory, filename);

	fs.writeFileSync(filePath, content);

	console.log(`File '${filename}' created in directory '${directory}' with the following content:`);
	console.log(content);
}

function replaceLineContent(filePath: string, lineNumber: number, oldContent: string, newContent: string): boolean {
	try {
		// 读取文件内容
		const fileContent = fs.readFileSync(filePath, 'utf-8');
		const lines = fileContent.split('\n');

		// 检查行号是否在有效范围内
		if (lineNumber <= 0 || lineNumber > lines.length) {
			console.error('Invalid line number');
			return false;
		}

		// 获取要替换的行的索引
		const lineIndex = lineNumber - 1;

		// 检查行的内容是否和原内容一致
		if (lines[lineIndex] === oldContent) {
			// 替换行的内容
			lines[lineIndex] = newContent;

			// 更新文件内容
			const newFileContent = lines.join('\n');
			fs.writeFileSync(filePath, newFileContent, 'utf-8');

			console.log('Replacement successful');
			return true;
		} else {
			console.log(`oldContent '${oldContent}'`);
			console.log(`lines[lineIndex] '${lines[lineIndex]}'`);
			console.error('Original content does not match');
			return false;
		}
	} catch (error) {
		console.error('Error:', error);
		return false;
	}
}

// This method is called when your extension is activated
// Your extension is activated the very first time the command is executed
export function activate(context: vscode.ExtensionContext) {

	// Use the console to output diagnostic information (console.log) and errors (console.error)
	// This line of code will only be executed once when your extension is activated
	console.log('Congratulations, your extension "zytcodestyle" is now active!');

	// The command has been defined in the package.json file
	// Now provide the implementation of the command with registerCommand
	// The commandId parameter must match the command field in package.json
	let disposable1 = vscode.commands.registerCommand('zytcodestyle.parse', async () => {
		const filePath = await promptForDirectoryPath();
		if (filePath) {
			vscode.window.showInformationMessage(`Selected file path: ${filePath}`);
			vscode.window.showInformationMessage("将为您解析目录生成 index.covrst 文件");
			const content = `jump_to_violation_1 violation_1 autosar A7-1-1 /home/dji/catkin_ws/src/zytcodestyle/testdir/main.cpp:5
jump_to_violation_2 violation_2 autosar A7-1-1 /home/dji/catkin_ws/src/zytcodestyle/testdir/main.cpp:10
jump_to_violation_3 violation_3 autosar A7-1-1 /home/dji/catkin_ws/src/zytcodestyle/testdir/main.cpp:10
jump_to_violation_4 violation_4 autosar A7-1-1 /home/dji/catkin_ws/src/zytcodestyle/testdir/main.cpp:10
jump_to_violation_5 violation_5 autosar A7-1-1 /home/dji/catkin_ws/src/zytcodestyle/testdir/main.cpp:10`;

			createAndWriteFile(filePath, 'index.covrst', content);
			goToPositionInFile(path.join(filePath, 'index.covrst'), 1);
		}
	});

	let disposable2 = vscode.commands.registerCommand('zytcodestyle.replace', (file: string, line: number, oldContent: string, newContent: string) => {
		if (replaceLineContent(file, line, oldContent, newContent)) {
			vscode.window.showInformationMessage('替换成功');
		} else {
			vscode.window.showInformationMessage('替换失败');
		}
	});

	context.subscriptions.push(disposable1);
	context.subscriptions.push(disposable2);


	vscode.commands.registerCommand('zytcodestyle.doMyCustomAction', () => {
		// do something with the clicked `word`
		vscode.window.showInformationMessage('已为您修复该编码规范违反项');
	});

	// 注册一个DefinitionProvider
	let disposable = vscode.languages.registerDefinitionProvider({ scheme: 'file' }, {
		provideDefinition(document, position, token) {
			// 获取光标位置的单词
			const wordRange = document.getWordRangeAtPosition(position);
			const word = document.getText(wordRange);

			// 检查单词是否符合指定格式
			const regex = /^violation_(\d+)$/;
			const match = word.match(regex);
			if (match) {
				const violationNumber = parseInt(match[1]);

				const { dir, base } = path.parse(document.fileName);
				// vscode.window.showInformationMessage(dir);
				// vscode.window.showInformationMessage(base);
				vscode.window.showInformationMessage('您选择了编码规范违反项' + violationNumber + '，正在为您跳转');
				return new vscode.Location(document.uri, position);
			}

			// 如果不符合格式，则不做任何反应
			return new vscode.Location(document.uri, position);
		}
	});
	context.subscriptions.push(disposable);

	// 注册一个HoverProvider
	let disposableHover = vscode.languages.registerHoverProvider({ scheme: 'file' }, {
		provideHover(document, position, token) {
			// 获取光标位置的单词
			const wordRange = document.getWordRangeAtPosition(position);
			const word = document.getText(wordRange);

			// 检查单词是否符合指定格式
			const regex = /^violation_(\d+)$/;
			const match = word.match(regex);
			if (match) {
				const violationNumber = parseInt(match[1]);

				const range = document.getWordRangeAtPosition(position);
				// 构建 Markdown 格式的提示信息
				const filename = '/tmp/1.txt';
				const lineNumber = 1;
				const oldContent = '123';
				const newContent = '1234';

				const markdownContent = new vscode.MarkdownString(`**智能修改提示**\n\n修改前：${oldContent}\n\n修改后：${newContent}\n\n[执行修改](command:zytcodestyle.replace?${encodeURIComponent(JSON.stringify([filename, lineNumber, oldContent, newContent]))})`);



				markdownContent.isTrusted = true;

				return new vscode.Hover(markdownContent, range);
			}
		}
	});
	context.subscriptions.push(disposableHover);



}

// This method is called when your extension is deactivated
export function deactivate() { }
