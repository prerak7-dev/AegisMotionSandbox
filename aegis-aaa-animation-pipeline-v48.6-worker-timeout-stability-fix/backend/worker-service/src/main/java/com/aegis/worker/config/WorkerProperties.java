package com.aegis.worker.config;

import org.springframework.boot.context.properties.ConfigurationProperties;

import java.io.File;

@ConfigurationProperties(prefix = "aegis.worker")
public class WorkerProperties {
    private String pipelineRoot = ".";
    private String powershellExe = "powershell.exe";
    private boolean usePowerShell = true;

    public String getPipelineRoot() { return pipelineRoot; }
    public void setPipelineRoot(String pipelineRoot) { this.pipelineRoot = pipelineRoot; }

    public String getPowershellExe() { return powershellExe; }
    public void setPowershellExe(String powershellExe) { this.powershellExe = powershellExe; }

    public boolean isUsePowerShell() { return usePowerShell; }
    public void setUsePowerShell(boolean usePowerShell) { this.usePowerShell = usePowerShell; }

    /**
     * Resolves the physical pipeline root regardless of where the Spring worker was launched from.
     *
     * The dashboard/worker can be started from backend/worker-service, from backend, from the repo
     * root, or from an IDE. In V47 the default pipeline-root value is ".", which previously made
     * scripts resolve to backend/worker-service/scripts/*.ps1 when the worker process cwd was the
     * worker module. This method accepts the configured value when it is already the root, otherwise
     * walks up parent directories until it finds the canonical pipeline layout:
     *
     *   <pipeline-root>/scripts
     *   <pipeline-root>/backend
     *
     * This keeps all PowerShell scripts running from <pipeline-root>/scripts and keeps their working
     * directory at the pipeline root.
     */
    public File resolvePipelineRootFile() {
        File configured = new File(normalizeRootValue(pipelineRoot)).getAbsoluteFile();
        File discoveredFromConfigured = findPipelineRoot(configured);
        if (discoveredFromConfigured != null) return discoveredFromConfigured;

        File cwd = new File(System.getProperty("user.dir", ".")).getAbsoluteFile();
        File discoveredFromCwd = findPipelineRoot(cwd);
        if (discoveredFromCwd != null) return discoveredFromCwd;

        return configured;
    }

    public String resolvePipelineRootPath() {
        return resolvePipelineRootFile().getAbsolutePath();
    }

    private static String normalizeRootValue(String value) {
        return (value == null || value.isBlank()) ? "." : value;
    }

    private static File findPipelineRoot(File start) {
        File cursor = start;
        for (int depth = 0; cursor != null && depth < 12; depth++) {
            File scriptsDir = new File(cursor, "scripts");
            File backendDir = new File(cursor, "backend");
            File workerModule = new File(backendDir, "worker-service");
            File markerScript = new File(scriptsDir, "02-extract-bandai-data.ps1");

            if (scriptsDir.isDirectory() && backendDir.isDirectory() && workerModule.isDirectory() && markerScript.isFile()) {
                return cursor.getAbsoluteFile();
            }
            cursor = cursor.getParentFile();
        }
        return null;
    }
}
