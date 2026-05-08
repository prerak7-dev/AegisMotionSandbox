package com.aegis.worker.service;

import com.aegis.common.model.JobState;
import com.aegis.common.model.JobStatus;
import com.aegis.common.model.PipelineStep;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;

import java.time.Instant;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Optional;

@Service
public class WorkerJobStore {
    private static final String KEY_PREFIX = "aegis:v47:job:";
    private static final String LEGACY_KEY_PREFIX = "aegis:v46:job:";

    private final StringRedisTemplate redis;
    private final ObjectMapper objectMapper;

    public WorkerJobStore(StringRedisTemplate redis, ObjectMapper objectMapper) {
        this.redis = redis;
        this.objectMapper = objectMapper;
    }

    private String key(String jobId) { return KEY_PREFIX + jobId; }
    private String legacyKey(String jobId) { return LEGACY_KEY_PREFIX + jobId; }
    private String latestExportKey() { return "aegis:v47:exports:latest"; }

    public Optional<JobState> find(String jobId) {
        String raw = redis.opsForValue().get(key(jobId));
        if (raw == null) raw = redis.opsForValue().get(legacyKey(jobId));
        if (raw == null) return Optional.empty();
        try {
            return Optional.of(objectMapper.readValue(raw, JobState.class));
        } catch (JsonProcessingException primary) {
            try {
                return Optional.of(jobStateFromMap(objectMapper.readValue(raw, new TypeReference<Map<String, Object>>() {}), raw));
            } catch (Exception fallback) {
                throw new IllegalStateException("Failed to deserialize job state from Redis. Raw value begins with: "
                        + raw.substring(0, Math.min(raw.length(), 500)), fallback);
            }
        }
    }

    public JobState require(String jobId) {
        return find(jobId).orElseThrow(() -> new IllegalArgumentException("Job not found: " + jobId));
    }

    public void save(JobState state) {
        state.setUpdatedAt(Instant.now());
        try {
            redis.opsForValue().set(key(state.getJobId()), objectMapper.writeValueAsString(state));
            if (state.getExportPath() != null && !state.getExportPath().isBlank()) {
                redis.opsForValue().set(latestExportKey(), state.getExportPath());
            }
        } catch (JsonProcessingException e) {
            throw new IllegalStateException(e);
        }
    }

    public void update(String jobId, PipelineStep step, JobStatus status, int progress, String message) {
        JobState state = require(jobId);
        state.setCurrentStep(step);
        state.setStatus(status);
        state.setProgress(progress);
        state.setMessage(message);
        state.appendLog("[" + step + "] " + message);
        save(state);
    }

    public void updateProgress(String jobId, int progress, String message) {
        JobState state = require(jobId);
        state.setProgress(Math.max(0, Math.min(100, progress)));
        state.setMessage(message);
        state.appendLog(message);
        save(state);
    }

    public void updateExportPath(String jobId, String exportPath) {
        if (exportPath == null || exportPath.isBlank()) return;
        JobState state = require(jobId);
        state.setExportPath(exportPath);
        state.appendLog("Export artifact: " + exportPath);
        save(state);
    }

    public void appendLog(String jobId, String line) {
        JobState state = require(jobId);
        state.appendLog(line);
        save(state);
    }

    private JobState jobStateFromMap(Map<String, Object> map, String raw) {
        JobState state = new JobState();
        state.setJobId(asString(map.get("jobId")));
        state.setStatus(parseEnum(JobStatus.class, map.get("status"), JobStatus.FAILED));
        state.setCurrentStep(parseEnum(PipelineStep.class, map.get("currentStep"), PipelineStep.V48_LOAD_GOLD_OVERLAY));
        state.setProgress(asInt(map.get("progress"), 0));
        state.setMessage(asString(map.get("message")));
        state.setConfigPath(asString(map.get("configPath")));
        state.setExportPath(asString(map.get("exportPath")));

        Object options = map.get("options");
        if (options instanceof Map<?, ?> rawOptions) {
            @SuppressWarnings("unchecked")
            Map<String, Object> typed = (Map<String, Object>) rawOptions;
            state.setOptions(typed);
        }

        Object logs = map.get("logs");
        if (logs instanceof List<?> rawLogs) {
            List<String> typedLogs = new ArrayList<>();
            for (Object item : rawLogs) typedLogs.add(String.valueOf(item));
            state.setLogs(typedLogs);
        } else {
            state.setLogs(new ArrayList<>(List.of("Redis job state fallback parser used.", raw)));
        }

        state.setCreatedAt(parseInstant(map.get("createdAt")));
        state.setUpdatedAt(parseInstant(map.get("updatedAt")));
        return state;
    }

    private static String asString(Object value) { return value == null ? null : String.valueOf(value); }
    private static int asInt(Object value, int fallback) {
        if (value instanceof Number n) return n.intValue();
        try { return value == null ? fallback : Integer.parseInt(String.valueOf(value)); }
        catch (Exception ignored) { return fallback; }
    }
    private static Instant parseInstant(Object value) {
        if (value instanceof Instant instant) return instant;
        if (value == null) return Instant.now();
        try { return Instant.parse(String.valueOf(value)); }
        catch (Exception ignored) { return Instant.now(); }
    }
    private static <T extends Enum<T>> T parseEnum(Class<T> type, Object value, T fallback) {
        if (value == null) return fallback;
        try { return Enum.valueOf(type, String.valueOf(value)); }
        catch (Exception ignored) { return fallback; }
    }
}
