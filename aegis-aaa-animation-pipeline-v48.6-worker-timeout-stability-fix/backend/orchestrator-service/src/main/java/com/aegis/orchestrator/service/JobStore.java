package com.aegis.orchestrator.service;

import com.aegis.common.model.JobState;
import com.aegis.common.model.JobStatus;
import com.aegis.common.model.PipelineStep;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;

import java.io.File;
import java.nio.file.Path;
import java.time.Duration;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;

@Service
public class JobStore {
    private static final String KEY_PREFIX = "aegis:v47:job:";
    private static final String LEGACY_KEY_PREFIX = "aegis:v46:job:";
    private static final Duration STALE_V48_VALIDATION_AFTER = Duration.ofMinutes(3);

    private final StringRedisTemplate redis;
    private final ObjectMapper objectMapper;

    public JobStore(StringRedisTemplate redis, ObjectMapper objectMapper) {
        this.redis = redis;
        this.objectMapper = objectMapper;
    }

    private String key(String jobId) { return KEY_PREFIX + jobId; }
    private String legacyKey(String jobId) { return LEGACY_KEY_PREFIX + jobId; }
    private String latestExportKey() { return "aegis:v47:exports:latest"; }

    public void save(JobState state) {
        state.setUpdatedAt(Instant.now());
        savePreservingUpdatedAt(state);
    }

    private void savePreservingUpdatedAt(JobState state) {
        try {
            redis.opsForValue().set(key(state.getJobId()), objectMapper.writeValueAsString(state));
            if (state.getExportPath() != null && !state.getExportPath().isBlank()) {
                redis.opsForValue().set(latestExportKey(), state.getExportPath());
            }
        } catch (JsonProcessingException e) {
            throw new IllegalStateException("Failed to serialize job state", e);
        }
    }

    public Optional<JobState> find(String jobId) {
        String raw = redis.opsForValue().get(key(jobId));
        if (raw == null) raw = redis.opsForValue().get(legacyKey(jobId));
        if (raw == null) return Optional.empty();
        JobState state;
        try {
            state = objectMapper.readValue(raw, JobState.class);
        } catch (JsonProcessingException primary) {
            try {
                state = jobStateFromMap(objectMapper.readValue(raw, new TypeReference<Map<String, Object>>() {}), raw);
            } catch (Exception fallback) {
                throw new IllegalStateException("Failed to deserialize job state from Redis. Raw value begins with: "
                        + raw.substring(0, Math.min(raw.length(), 500)), fallback);
            }
        }
        return Optional.of(reconcileStaleV48Validation(state));
    }

    public List<JobState> listAll() {
        List<JobState> jobs = new ArrayList<>();
        addMatchingKeys(jobs, KEY_PREFIX);
        addMatchingKeys(jobs, LEGACY_KEY_PREFIX);
        jobs.sort(Comparator.comparing(JobState::getCreatedAt, Comparator.nullsLast(Comparator.reverseOrder())));
        return jobs;
    }

    private void addMatchingKeys(List<JobState> jobs, String prefix) {
        Set<String> keys = redis.keys(prefix + "*");
        if (keys == null) return;
        for (String redisKey : keys) {
            String jobId = redisKey.substring(prefix.length());
            find(jobId).ifPresent(jobs::add);
        }
    }

    public List<String> tailLogs(String jobId, int limit) {
        JobState state = find(jobId).orElse(null);
        if (state == null || state.getLogs() == null) return Collections.emptyList();
        List<String> logs = state.getLogs();
        int safeLimit = Math.max(1, Math.min(limit, 500));
        int start = Math.max(0, logs.size() - safeLimit);
        return new ArrayList<>(logs.subList(start, logs.size()));
    }

    public boolean delete(String jobId) {
        Boolean deleted = redis.delete(key(jobId));
        Boolean deletedLegacy = redis.delete(legacyKey(jobId));
        return Boolean.TRUE.equals(deleted) || Boolean.TRUE.equals(deletedLegacy);
    }

    public Map<String, Object> deleteAll(boolean terminalOnly) {
        List<JobState> jobs = listAll();
        int deleted = 0;
        for (JobState job : jobs) {
            if (terminalOnly && !isTerminal(job.getStatus())) continue;
            if (delete(job.getJobId())) deleted++;
        }
        Map<String, Object> result = new LinkedHashMap<>();
        result.put("deleted", deleted);
        result.put("terminalOnly", terminalOnly);
        return result;
    }

    private boolean isTerminal(JobStatus status) {
        return status == JobStatus.COMPLETED || status == JobStatus.FAILED || status == JobStatus.CANCELLED;
    }

    public String latestExport() { return redis.opsForValue().get(latestExportKey()); }

    private JobState reconcileStaleV48Validation(JobState state) {
        if (state == null || isTerminal(state.getStatus())) return state;
        if (state.getCurrentStep() != PipelineStep.V48_VALIDATE_IMPORT_JSON) return state;
        if (state.getUpdatedAt() == null) return state;
        if (Duration.between(state.getUpdatedAt(), Instant.now()).compareTo(STALE_V48_VALIDATION_AFTER) < 0) return state;

        File validationFile = resolveValidationReportFile(state);
        if (validationFile != null && validationFile.isFile()) {
            try {
                JsonNode node = objectMapper.readTree(validationFile);
                if (node.path("valid").asBoolean(false)) {
                    state.setCurrentStep(PipelineStep.COMPLETE);
                    state.setStatus(JobStatus.COMPLETED);
                    state.setProgress(100);
                    state.setMessage("V48 pipeline complete. Existing validation report is valid; stale dashboard state reconciled.");
                    state.appendLog("[RECONCILE] Found valid validation report and marked job completed: " + validationFile.getPath());
                    savePreservingUpdatedAt(state);
                    return state;
                }
                state.setStatus(JobStatus.FAILED);
                state.setProgress(98);
                state.setMessage("V48 validation report exists but is invalid; job marked failed.");
                state.appendLog("[RECONCILE] Validation report exists but valid=false: " + validationFile.getPath());
                savePreservingUpdatedAt(state);
                return state;
            } catch (Exception e) {
                state.appendLog("[RECONCILE] Could not parse validation report " + validationFile.getPath() + ": " + e.getMessage());
            }
        }

        state.setStatus(JobStatus.FAILED);
        state.setProgress(98);
        state.setMessage("V48 validation did not complete within " + STALE_V48_VALIDATION_AFTER.toMinutes()
                + " minutes and no valid validation report was found. The worker timeout guard will prevent this job from blocking new jobs after restart.");
        state.appendLog("[RECONCILE] Marked stale V48 validation job failed. Validation report missing: "
                + (validationFile == null ? "unknown" : validationFile.getPath()));
        savePreservingUpdatedAt(state);
        return state;
    }

    private File resolveValidationReportFile(JobState state) {
        try {
            String exportPath = state.getExportPath();
            if (exportPath == null || exportPath.isBlank()) {
                exportPath = exportPathFromConfig(state.getConfigPath());
            }
            if (exportPath == null || exportPath.isBlank()) return null;
            File exportFile = new File(exportPath);
            if (!exportFile.isAbsolute()) exportFile = new File(resolvePipelineRoot(), exportPath);
            return new File(exportFile.getAbsolutePath() + ".validation.json");
        } catch (Exception ignored) {
            return null;
        }
    }

    private String exportPathFromConfig(String configPath) throws Exception {
        if (configPath == null || configPath.isBlank()) return null;
        File cfg = new File(configPath);
        if (!cfg.isAbsolute()) cfg = new File(resolvePipelineRoot(), configPath);
        if (!cfg.isFile()) return null;
        JsonNode node = objectMapper.readTree(cfg);
        JsonNode export = node.path("training").path("exportOutput");
        return export.isMissingNode() ? null : export.asText(null);
    }

    private File resolvePipelineRoot() {
        File current = new File(System.getProperty("user.dir", ".")).getAbsoluteFile();
        for (File cursor = current; cursor != null; cursor = cursor.getParentFile()) {
            if (new File(cursor, "scripts").isDirectory() && new File(cursor, "backend").isDirectory()) {
                return cursor;
            }
        }
        File parent = current.getParentFile();
        if (parent != null && new File(parent, "scripts").isDirectory() && new File(parent, "backend").isDirectory()) {
            return parent;
        }
        return current;
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
