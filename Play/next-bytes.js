// 1. Define numerical opcodes instead of objects
const OP = {
    LIT:  1,
    DUP:  2,
    ADD:  3,
    DROP: 4
};

// 2. Setup the Virtual Machine state with typed arrays
const MAX_PROGRAM_SIZE = 50000;
const program = new Uint8Array(MAX_PROGRAM_SIZE);
const dataStack = new Int32Array(1024); // Flat integer stack
let sp = -1; // Stack pointer
let ip = 0;  // Instruction pointer

// 3. Compile the same Forth sequence (LIT 1 DUP ADD DROP) into the byte array
let compileOffset = 0;
while (compileOffset < MAX_PROGRAM_SIZE - 5) {
    program[compileOffset++] = OP.LIT;
    program[compileOffset++] = 1; // Literal value (byte)
    program[compileOffset++] = OP.DUP;
    program[compileOffset++] = OP.ADD;
    program[compileOffset++] = OP.DROP;
}
const progLength = compileOffset;

// 4. Benchmark control variables
let totalNextCount = 0n;
const runDurationNs = 3_000_000_000n; // 3 seconds
const chunkSize = 5_000_000n;         // Bumped up because it's way faster now
const startTime = process.hrtime.bigint();

console.log("Running Byte Array + Switch Statement benchmark...");

while (true) {
    const elapsed = process.hrtime.bigint() - startTime;
    if (elapsed >= runDurationNs) break;

    // Tight inner interpreter loop
    for (let i = 0; i < chunkSize; i++) {
        if (ip >= progLength) ip = 0;

        // --- The Byte 'next' Fetch & Dispatch ---
        const opcode = program[ip++];

        switch (opcode) {
            case OP.LIT:
                dataStack[++sp] = program[ip++]; // Push literal directly from instruction stream
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
        }
    }

    totalNextCount += chunkSize;
}

const finalElapsedNs = process.hrtime.bigint() - startTime;
const finalElapsedSec = Number(finalElapsedNs) / 1_000_000_000;
const nextPerSecond = Number(totalNextCount) / finalElapsedSec;

console.log("\n--- Byte Array + Switch Results ---");
console.log(`Total Run Time:       ${finalElapsedSec.toFixed(4)} seconds`);
console.log(`Total 'next' Executed: ${totalNextCount.toLocaleString()}`);
console.log(`Operations/Second:     ${nextPerSecond.toLocaleString(undefined, { maximumFractionDigits: 0 })} Hz`);
