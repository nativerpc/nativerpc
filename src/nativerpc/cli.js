#!/usr/bin/env node
/**
 *  Native RPC Cli
 * 
 *      main
 */
const spawnSync = require('node:child_process').spawnSync;
const path = require('path');
const chalk = require('chalk').default;
const parts = process.argv.slice(2);
const command = parts[0];

function main() {
    const { Manager } = require('./manager');
    if (parts.includes('--help')) {
        console.log('Usage: nativerpc init|view')
    }
    else if (command == '--version' || command == '-v' || command == 'v') {
        console.log('v1.1.0')
    }
    else if (command === 'init' || command == 'i') {
        const manager = new Manager();
        manager.initWorkspace();
    }
    else if (command === 'initcpp') {
        const manager = new Manager();
        manager.initCpp();
    }
    else if (command === 'files' || command == 'f') {
        const manager = new Manager();
        manager.showFiles();
    }
    else if (command === 'types' || command == 't') {
        const manager = new Manager();
        manager.showTypes();
    }
    else if (command === 'ps' || command === 'processes' || command === 'p') {
        const manager = new Manager();
        manager.showProcesses(parts.includes('-f') || parts.includes('--follow'));
    }
    else if (command === 'parse') {
        const { stdout, stderr } = spawnSync(
            'python',
            [
                '-u',
                path.join(__dirname, 'parser.py'),
                parts[1]
            ]
        );
        console.log(chalk.magentaBright('Syntax Tree'));
        console.log(stderr.toString())
        console.log('');
        console.log(chalk.magentaBright('Schema List'));
        for (const line of stdout.toString().split('\n')) {
            console.log(`    ${line.trimEnd()}`)
        }
    }
    else {
        console.log('Usage: nativerpc init|files|types|ps')
    }
}

main();