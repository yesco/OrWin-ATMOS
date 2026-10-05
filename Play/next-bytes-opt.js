const OP = {
    RETURN: 0,
    LIT:    1,
    DUP:    2,
    ADD:    3,
    DROP:   4
};

const MAX_PROGRAM_SIZE = 1000;
const program = new Uint8Array(MAX_PROGRAM_SIZE);
const dataStack = new Int32Array(1024);
let sp = -1;
let ip = 0;

// Track exactly how many total 'next' operations are in our bytecode sequence
let opsPerRun = 0;
let compileOffset = 0;

for (let i = 0; i < 150; i++) {
    program[compileOffset++] = OP.LIT;  opsPerRun++;
    program[compileOffset++] = 5;       // Data payload (Not a 'next' op)
    program[compileOffset++] = OP.DUP;  opsPerRun++;
    program[compileOffset++] = OP.ADD;  opsPerRun++;
    program[compileOffset++] = OP.DROP; opsPerRun++;
}
program[compileOffset++] = OP.RETURN;   opsPerRun++; // The final exit next op

function interpret() {
    ip = 0;
    while (true) {
        switch (program[ip++]) {
            case OP.LIT:
                dataStack[++sp] = program[ip++];
                break;
            case OP.DUP:
                dataStack[sp + 1] = dataStack[sp];
                sp++;
                break;
            case OP.ADD:
                dataStack[sp - 1] = dataStack[sp - 1] + dataStack[sp];
                sp--;
                break;
            case OP.DROP:
                sp--;
                break;
            case OP.RETURN:
                return;
        }
    }
}

const ITERATIONS = 1000000;
console.log(`Executing ultra-tight interpreter loop ${ITERATIONS.toLocaleString()} times...`);

const startTime = process.hrtime.bigint();

for (let i = 0; i < ITERATIONS; i++) {
    interpret();
}

const finalElapsedNs = process.hrtime.bigint() - startTime;
const finalElapsedSec = Number(finalElapsedNs) / 1_000_000_000;

// Calculate the total ops by multiplying the static program size by the total runs
const totalNextExecuted = BigInt(opsPerRun) * BigInt(ITERATIONS);
const nextPerSecond = Number(totalNextExecuted) / finalElapsedSec;

console.log("\n--- Tight Loop Execution Results ---");
console.log(`Total Run Time:       ${finalElapsedSec.toFixed(6)} seconds`);
console.log(`Total 'next' Executed: ${totalNextExecuted.toLocaleString()}`);
console.log(`Operations/Second:     ${nextPerSecond.toLocaleString(undefined, { maximumFractionDigits: 0 })} Hz`);
