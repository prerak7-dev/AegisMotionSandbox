package com.aegis.orchestrator.service;

import com.aegis.common.dto.PipelineRunRequest;
import com.aegis.common.events.PipelineCommand;
import com.aegis.common.model.JobState;
import com.aegis.common.model.JobStatus;
import com.aegis.common.model.PipelineStep;
import org.springframework.stereotype.Service;

import java.time.Instant;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.UUID;

@Service
public class PipelineService {
    private final JobStore jobStore;
    private final PipelinePublisher publisher;

    public PipelineService(JobStore jobStore, PipelinePublisher publisher) {
        this.jobStore = jobStore;
        this.publisher = publisher;
    }

    public JobState startBandaiPipeline(PipelineRunRequest request) {
        String jobId = UUID.randomUUID().toString();

        Map<String, Object> options = new LinkedHashMap<>();
        String pipelineMode = request.shouldUseV48NoRetarget() ? "V48_QUATERNION_NO_RETARGET" : request.pipelineMode();
        options.put("pipelineMode", pipelineMode);
        options.put("skipTraining", request.shouldSkipTraining());
        options.put("action", request.action() == null || request.action().isBlank() ? "soccer_kick_overlay" : request.action());
        String style = request.kickStyle() != null && !request.kickStyle().isBlank() ? request.kickStyle() : request.style();
        options.put("style", style == null || style.isBlank() ? "instep_power_shot" : style);
        options.put("dominantLeg", request.dominantLeg() == null || request.dominantLeg().isBlank() ? "right" : request.dominantLeg());
        if (request.variantCount() != null && request.variantCount() > 0) options.put("variantCount", request.variantCount());
        if (request.durationSeconds() != null && request.durationSeconds() > 0.0) options.put("durationSeconds", request.durationSeconds());
        if (request.intensity() != null) options.put("intensity", request.intensity());
        if (request.followThrough() != null) options.put("followThrough", request.followThrough());
        if (request.plantStability() != null) options.put("plantStability", request.plantStability());
        if (request.upperBodyCounterbalance() != null) options.put("upperBodyCounterbalance", request.upperBodyCounterbalance());

        PipelineStep firstStep;
        String message;
        if ("V48_QUATERNION_NO_RETARGET".equalsIgnoreCase(pipelineMode)) {
            firstStep = PipelineStep.V48_LOAD_GOLD_OVERLAY;
            message = "Queued V48 quaternion no-retarget soccer kick pipeline";
        } else {
            options.put("skipClone", request.shouldSkipClone());
            options.put("skipRetarget", request.shouldSkipRetarget());
            options.put("skipConvert", request.shouldSkipConvert());
            options.put("skipUnrealImport", request.shouldSkipUnrealImport());
            options.put("pauseForManualRetarget", request.shouldPauseForManualRetarget());
            options.put("useOfflineRetarget", request.shouldUseOfflineRetarget());
            if (request.targetFbx() != null && !request.targetFbx().isBlank()) options.put("targetFbx", request.targetFbx());
            if (request.maxClips() != null && request.maxClips() > 0) options.put("maxClips", request.maxClips());
            firstStep = request.shouldSkipClone() ? PipelineStep.EXTRACT_BANDAI_DATA : PipelineStep.CLONE_BANDAI_REPO;
            message = "Queued legacy/experimental Bandai neural overlay pipeline";
        }

        JobState state = new JobState(jobId, JobStatus.QUEUED, firstStep, 0, message, request.configPath(), options);
        state.appendLog("Created job " + jobId + " mode=" + pipelineMode);
        if ("V48_QUATERNION_NO_RETARGET".equalsIgnoreCase(pipelineMode)) {
            state.appendLog("V48 production flow: V36 gold quaternion overlay → synthetic Manny/Quinn variants → quaternion tensor dataset → neural refinement → import JSON validation.");
            state.appendLog("Bandai/BVH/FBX/Unreal IK Retargeter steps are skipped completely in this mode.");
        }
        jobStore.save(state);

        publisher.publish(new PipelineCommand(jobId, firstStep, request.configPath(), options, Instant.now()));
        return state;
    }

    public JobState continueAfterRetarget(String jobId, String note) {
        JobState state = jobStore.find(jobId).orElseThrow(() -> new IllegalArgumentException("Job not found: " + jobId));
        state.setStatus(JobStatus.QUEUED);
        state.setCurrentStep(PipelineStep.EXPORT_RETARGETED_ANIMSEQUENCES);
        state.setMessage("Continuing legacy Unreal-retarget path after manual retarget");
        state.appendLog("Manual retarget confirmed. " + (note == null ? "" : note));
        jobStore.save(state);

        publisher.publish(new PipelineCommand(jobId, PipelineStep.EXPORT_RETARGETED_ANIMSEQUENCES, state.getConfigPath(), state.getOptions(), Instant.now()));
        return state;
    }
}
