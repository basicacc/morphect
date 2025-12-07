/**
 * Morphect - Ghidra Decompilation Script
 *
 * This script decompiles all functions in a binary and outputs
 * the decompiled C code for analysis. It's designed to be run
 * in Ghidra's headless mode.
 *
 * Usage:
 *   analyzeHeadless /project/dir ProjectName -import binary \
 *       -postScript DecompileAllFunctions.java
 *
 * Output is written to the script log.
 */

import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.util.task.TaskMonitor;

public class DecompileAllFunctions extends GhidraScript {

    @Override
    protected void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();

        try {
            // Initialize decompiler
            decompiler.openProgram(currentProgram);

            println("=== Morphect Decompilation Analysis ===");
            println("Binary: " + currentProgram.getName());
            println("");

            // Get function manager
            FunctionManager functionManager = currentProgram.getFunctionManager();

            // Iterate over all functions
            FunctionIterator functions = functionManager.getFunctions(true);

            int functionCount = 0;
            int decompileSuccessCount = 0;
            int totalStatements = 0;
            int totalBranches = 0;

            while (functions.hasNext() && !monitor.isCancelled()) {
                Function function = functions.next();
                String funcName = function.getName();

                // Skip thunks and external functions
                if (function.isThunk() || function.isExternal()) {
                    continue;
                }

                functionCount++;

                // Decompile the function
                DecompileResults results = decompiler.decompileFunction(
                    function,
                    30,  // timeout in seconds
                    monitor
                );

                if (results.decompileCompleted()) {
                    decompileSuccessCount++;

                    // Get decompiled code
                    DecompiledFunction decompiledFunc = results.getDecompiledFunction();
                    if (decompiledFunc != null) {
                        String decompiledCode = decompiledFunc.getC();

                        // Print function decompilation
                        println("=== Function: " + funcName + " ===");
                        println("Address: " + function.getEntryPoint());
                        println("");
                        println(decompiledCode);
                        println("");

                        // Calculate complexity metrics
                        int statements = countStatements(decompiledCode);
                        int branches = countBranches(decompiledCode);
                        totalStatements += statements;
                        totalBranches += branches;

                        println("Metrics: statements=" + statements +
                                ", branches=" + branches);
                        println("");
                    }
                } else {
                    println("=== Function: " + funcName + " ===");
                    println("DECOMPILATION FAILED: " + results.getErrorMessage());
                    println("");
                }
            }

            // Print summary
            println("=== Summary ===");
            println("Functions analyzed: " + functionCount);
            println("Successfully decompiled: " + decompileSuccessCount);
            println("Total statements: " + totalStatements);
            println("Total branches: " + totalBranches);

            if (functionCount > 0) {
                println("Average statements per function: " +
                        (totalStatements / functionCount));
                println("Average branches per function: " +
                        (totalBranches / functionCount));
            }

        } finally {
            decompiler.dispose();
        }
    }

    /**
     * Count approximate number of statements in decompiled code
     */
    private int countStatements(String code) {
        int count = 0;
        for (char c : code.toCharArray()) {
            if (c == ';') {
                count++;
            }
        }
        return count;
    }

    /**
     * Count branch statements (if, while, for, switch, case)
     */
    private int countBranches(String code) {
        int count = 0;

        // Count if statements
        int idx = 0;
        while ((idx = code.indexOf("if ", idx)) != -1) {
            count++;
            idx++;
        }

        // Count while loops
        idx = 0;
        while ((idx = code.indexOf("while ", idx)) != -1) {
            count++;
            idx++;
        }

        // Count for loops
        idx = 0;
        while ((idx = code.indexOf("for ", idx)) != -1) {
            count++;
            idx++;
        }

        // Count switch statements
        idx = 0;
        while ((idx = code.indexOf("switch ", idx)) != -1) {
            count++;
            idx++;
        }

        // Count case labels
        idx = 0;
        while ((idx = code.indexOf("case ", idx)) != -1) {
            count++;
            idx++;
        }

        return count;
    }
}
