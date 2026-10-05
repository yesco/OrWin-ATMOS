const primitives = {
    DUP:  { execute: (state) => { state.dataStack.push(state.dataStack[state.dataStack.length - 1]); } },
    DROP: { execute: (state) => { state.dataStack.pop(); } },
    ADD:  { execute: (state) => { const b = state.dataStack.pop(); state.dataStack[state.dataStack.length - 1] += b; } },
    LIT:  { execute: (state) => { state.dataStack.push(state.program[state.ip++].value); } },
};

const state = {
    dataStack: [],
    program: [],
    ip: 0
};

// Compile a large program sequence to minimize wrap-around checks
const loopSize = 5000;
for (let i = 0; i < loopSize; i++) {
    state.program.push(primitives.LIT);
    state.program.push({ value: 1 });
    state.program.push(primitives.DUP);
    state.program.push(primitives.ADD);
    state.program.push(primitives.DROP);
}

let totalNextCount = 0n;
const runDurationNs = 3_000_000_000n; // 3 seconds
const chunkSize = 1_000_000n;         // Execute 1 million 'next' ops per time check
const progLength = state.program.length;

console.log("Running optimized benchmark (chunked time checks)...");
const startTime = process.hrtime.bigint();

while (true) {
    // 1. Check time only once per chunk
    const elapsed = process.hrtime.bigint() - startTime;
    if (elapsed >= runDurationNs) break;

    // 2. Tight inner loop running purely the Forth interpreter 'next'
    for (let i = 0; i < chunkSize; i++) {
        if (state.ip >= progLength) state.ip = 0;

        // The Core 'next' Operation
        const word = state.program[state.ip++];
        word.execute(state);
    }

    totalNextCount += chunkSize;
}

const finalElapsedNs = process.hrtime.bigint() - startTime;
const finalElapsedSec = Number(finalElapsedNs) / 1_000_000_000;
const nextPerSecond = Number(totalNextCount) / finalElapsedSec;

console.log("\n--- Optimized Benchmark Results ---");
console.log(`Total Run Time:       ${finalElapsedSec.toFixed(4)} seconds`);
console.log(`Total 'next' Executed: ${totalNextCount.toLocaleString()}`);
console.log(`Operations/Second:     ${nextPerSecond.toLocaleString(undefined, { maximumFractionDigits: 0 })} Hz`);
