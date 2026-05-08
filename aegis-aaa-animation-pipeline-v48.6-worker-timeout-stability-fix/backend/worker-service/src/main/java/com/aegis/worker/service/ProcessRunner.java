package com.aegis.worker.service;

import com.aegis.worker.config.WorkerProperties;
import org.springframework.stereotype.Service;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;

@Service
public class ProcessRunner {
    private static final int EXIT_TIMEOUT = 124;

    private final WorkerProperties properties;

    public ProcessRunner(WorkerProperties properties) {
        this.properties = properties;
    }

    public int runPowerShellScript(String jobId, String scriptRelativePath, String configPath, Map<String, Object> options, WorkerJobStore jobStore) throws Exception {
        File root = properties.resolvePipelineRootFile();
        File script = new File(root, scriptRelativePath).getAbsoluteFile();

        if (!script.exists()) {
            throw new IllegalArgumentException("Script not found: " + script
                    + " | resolved pipeline root=" + root
                    + " | cwd=" + new File(System.getProperty("user.dir", ".")).getAbsoluteFile()
                    + " | set AEGIS_PIPELINE_ROOT to the folder that contains scripts/ and backend/ if this is unexpected.");
        }

        List<String> command = new ArrayList<>();
        command.add(properties.getPowershellExe());
        command.add("-NoProfile");
        command.add("-ExecutionPolicy");
        command.add("Bypass");
        command.add("-File");
        command.add(script.getAbsolutePath());

        if (configPath != null && !configPath.isBlank()) {
            command.add("-ConfigPath");
            command.add(configPath);
        }
        if (options != null && scriptRelativePath.contains("offline-retarget")) {
            addOptionArg(command, "-TargetFbx", options.get("targetFbx"));
            addOptionArg(command, "-MaxClips", options.get("maxClips"));
        }

        if (options != null && scriptRelativePath.contains("generate-bandai-soccer-kick-overlay")) {
            addOptionArg(command, "-Action", options.get("action"));
            addOptionArg(command, "-Style", options.get("style"));
            addOptionArg(command, "-DominantLeg", options.get("dominantLeg"));
            addOptionArg(command, "-Duration", options.get("durationSeconds"));
        }

        if (options != null && scriptRelativePath.contains("v48-02-generate-quaternion-variants")) {
            addOptionArg(command, "-Style", options.get("style"));
            addOptionArg(command, "-DominantLeg", options.get("dominantLeg"));
            addOptionArg(command, "-VariantCount", options.get("variantCount"));
            addOptionArg(command, "-Intensity", options.get("intensity"));
            addOptionArg(command, "-FollowThrough", options.get("followThrough"));
            addOptionArg(command, "-PlantStability", options.get("plantStability"));
            addOptionArg(command, "-UpperBodyCounterbalance", options.get("upperBodyCounterbalance"));
        }

        if (options != null && scriptRelativePath.contains("v48-05-generate-import-json")) {
            addOptionArg(command, "-Style", options.get("style"));
            addOptionArg(command, "-DominantLeg", options.get("dominantLeg"));
            addOptionArg(command, "-Duration", options.get("durationSeconds"));
            addOptionArg(command, "-Intensity", options.get("intensity"));
            addOptionArg(command, "-FollowThrough", options.get("followThrough"));
            addOptionArg(command, "-PlantStability", options.get("plantStability"));
            addOptionArg(command, "-UpperBodyCounterbalance", options.get("upperBodyCounterbalance"));
        }

        ProcessBuilder pb = new ProcessBuilder(command);
        pb.directory(root);
        pb.redirectErrorStream(true);

        long timeoutSeconds = timeoutSecondsFor(scriptRelativePath);
        jobStore.appendLog(jobId, "Pipeline root: " + root.getAbsolutePath());
        jobStore.appendLog(jobId, "Running: " + String.join(" ", command));
        jobStore.appendLog(jobId, "Step timeout: " + timeoutSeconds + "s");
        Process process = pb.start();

        ExecutorService outputExecutor = Executors.newSingleThreadExecutor(r -> {
            Thread t = new Thread(r, "aegis-step-output-" + jobId.substring(0, Math.min(8, jobId.length())));
            t.setDaemon(true);
            return t;
        });

        Future<?> outputFuture = outputExecutor.submit(() -> {
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    jobStore.appendLog(jobId, line);
                    handleProgressLine(jobId, line, jobStore);
                }
            } catch (Exception e) {
                try {
                    jobStore.appendLog(jobId, "[PROCESS_OUTPUT_READER] " + e.getMessage());
                } catch (Exception ignored) {
                    // The job may have been deleted while the process was running.
                }
            }
        });

        boolean finished = process.waitFor(timeoutSeconds, TimeUnit.SECONDS);
        if (!finished) {
            jobStore.appendLog(jobId, "[TIMEOUT] Step exceeded " + timeoutSeconds + "s. Terminating PowerShell/Python process tree so this job cannot block the worker queue.");
            destroyProcessTree(process);
            outputFuture.cancel(true);
            outputExecutor.shutdownNow();
            jobStore.appendLog(jobId, "Exit code: " + EXIT_TIMEOUT + " (timeout)");
            return EXIT_TIMEOUT;
        }

        try {
            outputFuture.get(10, TimeUnit.SECONDS);
        } catch (Exception ignored) {
            outputFuture.cancel(true);
        } finally {
            outputExecutor.shutdownNow();
        }

        int exit = process.exitValue();
        jobStore.appendLog(jobId, "Exit code: " + exit);
        return exit;
    }

    private long timeoutSecondsFor(String scriptRelativePath) {
        String override = System.getenv("AEGIS_STEP_TIMEOUT_SECONDS");
        if (override != null && !override.isBlank()) return parseLong(override, 900L);

        String script = scriptRelativePath == null ? "" : scriptRelativePath.toLowerCase();
        if (script.contains("v48-06-validate") || script.contains("verify-overlay") || script.contains("validate")) {
            return parseLong(System.getenv("AEGIS_VALIDATION_TIMEOUT_SECONDS"), 180L);
        }
        if (script.contains("v48-04-train") || script.contains("train-")) {
            return parseLong(System.getenv("AEGIS_TRAINING_TIMEOUT_SECONDS"), 1800L);
        }
        if (script.contains("v48-02-generate") || script.contains("v48-03-build") || script.contains("v48-05-generate")) {
            return parseLong(System.getenv("AEGIS_V48_STEP_TIMEOUT_SECONDS"), 600L);
        }
        return 900L;
    }

    private long parseLong(String value, long fallback) {
        try {
            if (value == null || value.isBlank()) return fallback;
            return Long.parseLong(value.trim());
        } catch (Exception ignored) {
            return fallback;
        }
    }

    private void destroyProcessTree(Process process) {
        ProcessHandle handle = process.toHandle();
        List<ProcessHandle> descendants = handle.descendants()
                .sorted(Comparator.comparingLong(ProcessHandle::pid).reversed())
                .toList();
        for (ProcessHandle child : descendants) {
            try { child.destroy(); } catch (Exception ignored) {}
        }
        try { handle.destroy(); } catch (Exception ignored) {}
        try { Thread.sleep(1500); } catch (InterruptedException ignored) { Thread.currentThread().interrupt(); }
        for (ProcessHandle child : descendants) {
            try { if (child.isAlive()) child.destroyForcibly(); } catch (Exception ignored) {}
        }
        try { if (handle.isAlive()) handle.destroyForcibly(); } catch (Exception ignored) {}
    }

    private void addOptionArg(List<String> command, String flag, Object value) {
        if (value != null && !String.valueOf(value).isBlank()) {
            command.add(flag);
            command.add(String.valueOf(value));
        }
    }

    private void handleProgressLine(String jobId, String line, WorkerJobStore jobStore) {
        if (line == null || !line.startsWith("AEGIS_PROGRESS|")) return;
        Map<String, String> values = new HashMap<>();
        String[] parts = line.split("\\|");
        for (String part : parts) {
            int idx = part.indexOf('=');
            if (idx > 0 && idx < part.length() - 1) values.put(part.substring(0, idx), part.substring(idx + 1));
        }
        try {
            int progress = Integer.parseInt(values.getOrDefault("progress", "0"));
            String message = values.getOrDefault("message", line);
            jobStore.updateProgress(jobId, progress, message);
        } catch (Exception ignored) {
            // Progress messages should never fail the worker.
        }
    }
}
