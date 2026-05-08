package com.aegis.common.model;

import java.time.Instant;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

public class JobState {
    private String jobId;
    private JobStatus status;
    private PipelineStep currentStep;
    private int progress;
    private String message;
    private String configPath;
    private Map<String, Object> options;
    private List<String> logs = new ArrayList<>();
    private String exportPath;
    private Instant createdAt;
    private Instant updatedAt;

    public JobState() {}

    public JobState(String jobId, JobStatus status, PipelineStep currentStep, int progress, String message,
                    String configPath, Map<String, Object> options) {
        this.jobId = jobId;
        this.status = status;
        this.currentStep = currentStep;
        this.progress = progress;
        this.message = message;
        this.configPath = configPath;
        this.options = options;
        this.createdAt = Instant.now();
        this.updatedAt = Instant.now();
    }

    public String getJobId() { return jobId; }
    public void setJobId(String jobId) { this.jobId = jobId; }

    public JobStatus getStatus() { return status; }
    public void setStatus(JobStatus status) { this.status = status; }

    public PipelineStep getCurrentStep() { return currentStep; }
    public void setCurrentStep(PipelineStep currentStep) { this.currentStep = currentStep; }

    public int getProgress() { return progress; }
    public void setProgress(int progress) { this.progress = progress; }

    public String getMessage() { return message; }
    public void setMessage(String message) { this.message = message; }

    public String getConfigPath() { return configPath; }
    public void setConfigPath(String configPath) { this.configPath = configPath; }

    public Map<String, Object> getOptions() { return options; }
    public void setOptions(Map<String, Object> options) { this.options = options; }

    public List<String> getLogs() { return logs; }
    public void setLogs(List<String> logs) { this.logs = logs; }

    public String getExportPath() { return exportPath; }
    public void setExportPath(String exportPath) { this.exportPath = exportPath; }

    public Instant getCreatedAt() { return createdAt; }
    public void setCreatedAt(Instant createdAt) { this.createdAt = createdAt; }

    public Instant getUpdatedAt() { return updatedAt; }
    public void setUpdatedAt(Instant updatedAt) { this.updatedAt = updatedAt; }

    public void appendLog(String line) {
        this.logs.add(line);
        this.updatedAt = Instant.now();
    }
}
