/**
 *  Native RPC Manager
 * 
 *      Manager
 *          constructor
 *          getSettings
 *          getFiles
 *          getSchema
 *          getMetadata
 * 
 *          initWorkspace
 *          showFiles
 *          showTypes
 *          showProcesses
 * 
 *          fetchWithAbort
 *          zipArrays
 *          printText
 *          printLine
 *          initConsole
 *          moveUp
 *          eraseLine
 */
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const spawnSync = require('node:child_process').spawnSync;
const chalk = require('chalk').default;
const { CONFIG_NAME, SchemaInfo, verifyPython, SchemaFileInfo, QueryInfo } = require('./common');
const TEST_MODE = false;


class Manager {
    constructor() {
        this.schemaFiles = [];
        this.schemaList = [];
        this.queryList = [];

        // Settings
        this.workspaceRoot = null;
        this.workspaceFile = null;
        this.projectNames = [];
        this.pythonCommand = null;
        this.schemaNames = null;
        this.primaryPorts = null;
        this.followProcesses = null;
    }

    getSettings() {
        let curDir = process.cwd();
        let workspaceFile = null;
        while (true) {
            if (fs.existsSync(path.join(curDir, CONFIG_NAME))) {
                workspaceFile = path.join(curDir, CONFIG_NAME);
                break;
            }
            curDir = path.dirname(curDir);
            if (!path.basename(curDir)) {
                break;
            }
        }
        this.workspaceRoot = workspaceFile ? path.dirname(workspaceFile) : process.cwd();
        this.workspaceFile = workspaceFile;
        const settings = workspaceFile ? JSON.parse(fs.readFileSync(
            workspaceFile,
            { encoding: 'utf-8' }
        )) : {};
        this.projectNames = settings.projectNames ?? [];
        this.pythonCommand = settings.pythonCommand ?? "python";
        this.schemaNames = settings.schemaNames ?? [
            "common"
        ];
        this.primaryPorts = settings.primaryPorts ?? [
            9001,
            9002,
            9003
        ];
        this.followProcesses = settings.followProcesses ?? false;
    }

    getFiles(deepSearch) {
        let projectNames = null;

        if (!deepSearch && !this.workspaceFile) {
            console.log(`Missing workspace configuration. Run ${chalk.yellow('nativerpc init')} to configure.`)
            process.exit(1);
        }

        if (deepSearch) {
            projectNames = [];
            const entries = fs.readdirSync(this.workspaceRoot, { withFileTypes: true });

            for (const entry of entries) {
                if (!entry.isDirectory()) {
                    continue;
                }
                if (entry.name == 'node_modules' || entry.name == 'venv' || entry.name == 'build' || entry.name == '__pycache__' || entry.name.startsWith('_') || entry.name.startsWith('.')) {
                    continue;
                }
                const projectDir = path.join(this.workspaceRoot, entry.name);
                projectNames.push(entry.name);
            }
        } else {
            projectNames = this.projectNames;
        }

        for (const projectName of projectNames) {
            const projectDir = path.join(this.workspaceRoot, projectName);
            const projFiles = ['package.json', "pyproject.toml", 'CMakeLists.txt'].filter(item => fs.existsSync(path.join(projectDir, item)));
            const sourceExtensions = [".ts", ".py", ".h"];
            if (projFiles.includes('package.json')) {
                if (JSON.parse(fs.readFileSync(path.join(projectDir, 'package.json'))).name == 'nativerpc') {
                    continue;
                }
            }

            function read(schemaNames, projectDir) {
                const result = [];
                const entries = fs.readdirSync(projectDir, { withFileTypes: true });

                for (const entry of entries) {
                    if (entry.name == 'node_modules' || entry.name == 'venv' || entry.name == 'build' || entry.name == '__pycache__' || entry.name.startsWith('.')) {
                        continue;
                    }
                    const baseName = entry.name.substring(0, entry.name.lastIndexOf('.'));
                    const extension = entry.name.substring(entry.name.lastIndexOf('.'));
                    if (entry.isFile() && schemaNames.includes(baseName) && sourceExtensions.includes(extension)) {
                        result.push(new SchemaFileInfo({
                            projectName: projectName,
                            projFiles: projFiles,
                            schemaFile: path.join(entry.parentPath, entry.name)
                        }));
                    }
                    else if (entry.isDirectory()) {
                        for (const item of read(schemaNames, path.join(entry.parentPath, entry.name))) {
                            result.push(item);
                        }
                    }
                }
                return result;
            }

            this.schemaFiles = [
                ...this.schemaFiles,
                ...read(this.schemaNames, projectDir)
            ];
        }
    }

    getSchema() {
        this.schemaList = [];
        for (const file of this.schemaFiles) {
            assert(path.join(__dirname, 'parser.py'));
            assert(fs.existsSync(file.schemaFile));
            const { status, stdout, stderr } = spawnSync(
                'python',
                [
                    '-u',
                    path.join(__dirname, 'parser.py'),
                    file.schemaFile,
                ]
            );
            // console.log(stdout.toString())
            // console.log(stderr.toString())
            assert(status === 0);
            for (const item of JSON.parse(stdout.toString())) {
                this.schemaList.push(new SchemaInfo({
                    projectName: file.projectName,
                    className: item.className,
                    fieldName: item.fieldName,
                    fieldType: item.fieldType,
                    methodName: item.methodName,
                    methodRequest: item.methodRequest,
                    methodResponse: item.methodResponse,
                    idNumber: item.idNumber,
                }));
            }
        }
    }

    async getMetadata() {
        const controller = new AbortController();
        const result = []
        for (const port of this.primaryPorts) {
            result.push(new QueryInfo({
                port,
                promise: this.fetchWithAbort(`http://localhost:${port}/Metadata/getMetadata`, {}, controller),
                queryResult: null,
            }));
        }
        await new Promise((resolve) => { setTimeout(() => { resolve(1) }, 100) });
        controller.abort()
        for (const item of result) {
            item.queryResult = await item.promise;
            if (TEST_MODE) {
                item.queryResult = {
                    clientInfos: [
                        { stime: Date.now() / 1000 - 5000, projectId: 'test_python', callId: '/App/hello', active: true },
                        { stime: Date.now() / 1000 - 5000, projectId: 'test_python2', callId: '/App/hello', active: true },
                        { stime: Date.now() / 1000 - 15000, projectId: 'test_python2', callId: '/App/hello', active: false },
                    ]
                };
            }
        }
        this.queryList = result;
    }

    initWorkspace() {
        verifyPython();
        this.getSettings();
        this.getFiles(true);
        const names = [...new Set(this.schemaFiles.map(item => item.projectName))];
        const settings = this.workspaceFile ? JSON.parse(fs.readFileSync(
            this.workspaceFile,
            { encoding: 'utf-8' }
        )) : {};
        const workspaceFile = this.workspaceFile || path.join(this.workspaceRoot, CONFIG_NAME);
        settings['projectNames'] = names;
        fs.writeFileSync(
            workspaceFile,
            JSON.stringify(settings, undefined, 2),
            { encoding: 'utf-8' }
        );

        console.log(`Found ${names.length} projects.`)
        console.log(`Updated configuration in ${workspaceFile}.`)
    }

    async showFiles() {
        verifyPython();
        this.getSettings();
        this.getFiles(false);
        this.getSchema();
        const lines = [];

        lines.push(chalk.dim('FOLDER                 PROJECT             SCHEMA              SERVICES       MESSAGES'))
        for (const projectName of this.projectNames) {
            const schemaFiles = this.schemaFiles.filter(item => item.projectName === projectName);
            assert(schemaFiles.length == 1);
            const projFile = schemaFiles[0] && schemaFiles[0].projFiles.length > 0 ? schemaFiles[0].projFiles[0] : '-';
            assert(schemaFiles && schemaFiles.length > 0);
            const methods = this.schemaList.filter(item => item.projectName === projectName && item.methodName).map(item => [item.className, item.methodName]);
            const fields = this.schemaList.filter(item => item.projectName === projectName && item.fieldName).map(item => [item.className, item.fieldName]);
            const serviceNames = [...new Set(methods.map(item => item[0]))].join(', ');
            const typeNames = [...new Set(fields.map(item => item[0]))].map(item => chalk.green(item)).join(', ');
            const schemaFile = path.basename(schemaFiles[0].schemaFile);
            lines.push(
                `${projectName.padEnd(22, ' ')} ` +
                `${projFile.padEnd(19, ' ')} ` +
                `${schemaFile.padEnd(19, ' ')} ` +
                `${chalk.yellow(serviceNames).padEnd(24, ' ')} ` +
                `${typeNames.padEnd(15, ' ')}`
            )
        }

        this.printText(lines);
    }

    async showTypes() {
        verifyPython();
        this.getSettings();
        this.getFiles(false);
        this.getSchema();
        const lines = [];
        const bestCount = this.projectNames.length;

        const fields2 = [...new Set(this.schemaList.filter(item => item.fieldName).map(item => item.className + ':' + item.fieldName + ':' + item.fieldType))];
        fields2.sort((a, b) => a.split(':')[0] === b.split(':')[0] ? a.split(':')[1].localeCompare(b.split(':')[1]) : a.split(':')[0].localeCompare(b.split(':')[0]))

        lines.push(chalk.dim('CLASS                METHOD/FIELD           TYPE                      COUNT'))

        const methods2 = [...this.schemaList.filter(item => item.methodName)];
        for (let i = 0; i < methods2.length; i++) {
            for (let j = i + 1; j < methods2.length; j++) {
                const item1 = methods2[i];
                const item2 = methods2[j];
                if (item1.className == item2.className && item1.methodName == item2.methodName && item1.methodRequest === item2.methodRequest && item1.methodReponse === item2.methodReponse) {
                    methods2.splice(j, 1);
                    j--;
                }
            }
        }
        methods2.sort((a, b) => `${a.className}.${a.methodName}`.localeCompare(`${b.className}.${b.methodName}`))

        let lastName = '';
        for (const item of methods2) {
            const count = this.schemaList.filter(x => x.className == item.className && x.methodName == item.methodName && x.methodRequest === item.methodRequest && x.methodResponse === item.methodResponse).length;
            const status = count == bestCount ? chalk.magenta(`${count}/${bestCount}`) : chalk.dim(chalk.magenta(`${count}/${bestCount}`));
            const signature = `${chalk.cyan(item.methodRequest)} ${chalk.dim('->')} ${chalk.cyan(item.methodResponse)}`;
            lines.push(
                `${(lastName === item.className ? ' ' : chalk.yellow(item.className)).padEnd(20 + (lastName == item.className ? 0 : 10), ' ')} ` +
                `${chalk.blue(item.methodName).padEnd(32, ' ')} ` +
                `${signature.padEnd(34 + 20, ' ')} ` +
                `${status}`
            )
            lastName = item.className;
        }

        lastName = '';
        for (const item of fields2) {
            const [className, fieldName, fieldType] = item.split(':');
            const count = this.schemaList.filter(item => item.className == className && item.fieldName == fieldName && item.fieldType === fieldType).length;
            const status = count == bestCount ? chalk.magenta(`${count}/${bestCount}`) : chalk.dim(chalk.magenta(`${count}/${bestCount}`));
            const className3 = lastName == className ? ' ' : chalk.green(className);
            lines.push(
                `${className3.padEnd(20 + (lastName == className ? 0 : 10), ' ')} ` +
                `${chalk.blue(fieldName).padEnd(32, ' ')} ` +
                `${chalk.cyan(fieldType).padEnd(35, ' ')} ${status}`
            )
            lastName = className;
        }

        this.printText(lines)
    }


    async showProcesses(follow) {
        verifyPython();
        this.getSettings();
        this.getFiles(false);
        this.getSchema();
        let spin = 0;
        let lastLines = [];
        if (follow || this.followProcesses) {
            this.initConsole(true);
        }

        while (true) {
            spin++;
            await this.getMetadata()
            const lines = [];

            lines.push(chalk.dim('PORT         TYPE           SERVICE                ALIVE             LAST CALL'))
            for (const item of this.queryList) {
                if (!item.queryResult) {
                    lines.push(`${item.port.toString().padEnd(12, ' ')} ${chalk.dim('-              -                      -                 -')}`)
                    continue;
                }
                let index = -1;
                for (const item2 of item.queryResult.clientInfos) {
                    index++;
                    const portStr = index == 0 ? `${item.port}` : ' ';
                    const typeStr1 = index == 0 ? `SERVER` : 'CLIENT';
                    const typeStr = !item2.active ? chalk.dim(typeStr1) : typeStr1;
                    const elapsedSec1 = (Date.now() / 1000.0 - item2.stime).toFixed(0);
                    const elapsedSec = !item2.active ? chalk.dim(elapsedSec1) : elapsedSec1;
                    const projId = !item2.active ? chalk.dim(item2.projectId) : item2.projectId;
                    const msgtype3 = item2.callId.split('/');
                    const msgtype5 = msgtype3[msgtype3.length - 2]
                    const msgtype4 = msgtype3[msgtype3.length - 1]
                    const msgtype6 = `${chalk.yellow(msgtype5)}.${chalk.blue(msgtype4)}`;
                    const msgtype7 = !item2.active ? chalk.dim(msgtype6) : msgtype6;
                    lines.push(
                        `${portStr.padEnd(12 + (index == 0 ? 0 : 0), ' ')} ` +
                        `${typeStr.padEnd(14 + (item2.active ? 0 : 9), ' ')} ` +
                        `${projId.padEnd(22 + (item2.active ? 0 : 9), ' ')} ` +
                        `${(elapsedSec + chalk.dim(' sec')).padEnd(26 + (item2.active ? 0 : 9), ' ')} ` +
                        `${msgtype7}`
                    )
                }
            }

            if (!follow && !this.followProcesses) {
                this.printText(lines);
                break;
            }

            this.printLine('\r');
            for (const item of lastLines.reverse()) {
                this.moveUp();
                this.printLine('\r');
                this.eraseLine(false, item.length)
            }
            this.printLine('\r');
            lastLines = []
            for (const item of lines) {
                lastLines.push(item);
                this.printLine(item)
                this.printLine('\n')
            }
            const SPINNER_TEXT = ['-', '\\', '|', '/']
            const spinnerText = '' + SPINNER_TEXT[spin % SPINNER_TEXT.length];
            this.printLine(spinnerText)
            this.printLine('\n');
            this.eraseLine(true, -1)
            lastLines = [...lines, spinnerText];
            await new Promise((resolve) => { setTimeout(() => { resolve(1) }, 500) });
        }
    }

    async fetchWithAbort(url, body, controller) {
        try {
            const resp = await fetch(
                url,
                {
                    method: 'POST',
                    headers: {
                        'Project-Id': 'nativerpc',
                        'Sender-Id': 'fetch',
                    },
                    body: JSON.stringify(body),
                    signal: controller.signal
                }
            );
            return await resp.json();
        }
        catch (ex) {
        }
        return null;
    }

    zipArrays(arr1, arr2) {
        const maxLength = Math.max(arr1.length, arr2.length);
        const result = [];

        for (let i = 0; i < maxLength; i++) {
            result.push([arr1[i], arr2[i]]);
        }
        return result;
    }

    printText(lines) {
        for (const item of lines) {
            console.log(item);
        }
    }

    printLine(n) {
        process.stdout.write(n);
    }

    initConsole(hideCursor) {
        const CSI_CODE = Buffer.from([0x1b, 0x5b]);
        process.on('SIGINT', () => {
            if (this.wasHidden) {
                process.stdout.write(CSI_CODE + '?25h')
            }
            console.log('Quit');
            process.exit();
        });
        this.wasHidden = false;
        if (hideCursor) {
            this.wasHidden = true;
            process.stdout.write(CSI_CODE + '?25l')
        }
    }

    moveUp() {
        const CSI_CODE = Buffer.from([0x1b, 0x5b]);
        process.stdout.write(CSI_CODE + 'A')
    }

    eraseLine(full, n) {
        const CSI_CODE = Buffer.from([0x1b, 0x5b]);
        if (full) {
            process.stdout.write(CSI_CODE + 'K')
        } else {
            process.stdout.write(new Array(n + 1).join(' '));
        }
    }
}

module.exports = {
    Manager
};