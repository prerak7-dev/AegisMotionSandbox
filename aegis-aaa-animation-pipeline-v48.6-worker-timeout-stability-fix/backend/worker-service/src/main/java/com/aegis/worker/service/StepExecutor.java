package com.aegis.worker.service;

import com.aegis.common.events.PipelineCommand;
import com.aegis.common.model.JobStatus;
import com.aegis.common.model.PipelineStep;
import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.aegis.worker.config.WorkerProperties;
import org.springframework.stereotype.Service;

import java.io.File;
import java.nio.file.Path;
import java.util.Map;

@Service
public class StepExecutor {
    private final ProcessRunner runner;
    private final WorkerJobStore jobStore;
    private final ObjectMapper objectMapper;
    private final WorkerProperties workerProperties;

    public StepExecutor(ProcessRunner runner, WorkerJobStore jobStore, ObjectMapper objectMapper, WorkerProperties workerProperties) {
        this.runner = runner;
        this.jobStore = jobStore;
        this.objectMapper = objectMapper;
        this.workerProperties = workerProperties;
    }

    public boolean execute(PipelineCommand command) throws Exception {
        PipelineStep step = command.step();
        if (step == PipelineStep.WAIT_FOR_MANUAL_RETARGET) {
            jobStore.update(command.jobId(), step, JobStatus.WAITING_FOR_RETARGET, 50,
                    "Waiting for manual IK retarget in Unreal. V48 production mode does not use this path.");
            return false;
        }
        if (step == PipelineStep.COMPLETE) {
            jobStore.update(command.jobId(), step, JobStatus.COMPLETED, 100, "Pipeline complete");
            return false;
        }
        String script = scriptFor(step);
        jobStore.update(command.jobId(), step, JobStatus.RUNNING, progressFor(step), "Executing " + script);
        int exit = runner.runPowerShellScript(command.jobId(), script, command.configPath(), command.options(), jobStore);
        if (exit != 0) {
            jobStore.update(command.jobId(), step, JobStatus.FAILED, progressFor(step), "Step failed with exit code " + exit);
            return false;
        }
        if (step == PipelineStep.GENERATE_OVERLAY_JSON || step == PipelineStep.VERIFY_OVERLAY_JSON || step == PipelineStep.V48_GENERATE_IMPORT_JSON || step == PipelineStep.V48_VALIDATE_IMPORT_JSON) {
            String exportPath = resolveExportPath(command.configPath());
            if (exportPath != null) jobStore.updateExportPath(command.jobId(), exportPath);
        }

        // Minimal V48.5 rollback fix: keep the stable V48.1 queue/worker flow, but
        // complete the V48 job immediately after validation succeeds. This avoids
        // the old dashboard hang at the final validation step without changing
        // Kafka dispatch, worker heartbeat, Redis queues, or dashboard requeue logic.
        if (step == PipelineStep.V48_VALIDATE_IMPORT_JSON) {
            jobStore.update(command.jobId(), PipelineStep.COMPLETE, JobStatus.COMPLETED, 100,
                    "V48 pipeline complete. Final quaternion import JSON validated successfully.");
            return false;
        }

        jobStore.update(command.jobId(), step, JobStatus.RUNNING, progressFor(step), "Step completed");
        return true;
    }

    public int progressFor(PipelineStep step) {
        return switch (step) {
            case V48_LOAD_GOLD_OVERLAY -> 8;
            case V48_GENERATE_SYNTHETIC_VARIANTS -> 25;
            case V48_BUILD_QUATERNION_DATASET -> 45;
            case V48_TRAIN_QUATERNION_PRIOR -> 72;
            case V48_GENERATE_IMPORT_JSON -> 92;
            case V48_VALIDATE_IMPORT_JSON -> 98;
            case CLONE_BANDAI_REPO -> 5;
            case EXTRACT_BANDAI_DATA -> 10;
            case SELECT_BANDAI_CLIPS -> 16;
            case OFFLINE_RETARGET_TO_MANNY_JSON -> 35;
            case CONVERT_BVH_TO_FBX -> 30;
            case IMPORT_FBX_TO_UNREAL -> 42;
            case WAIT_FOR_MANUAL_RETARGET -> 50;
            case EXPORT_RETARGETED_ANIMSEQUENCES -> 60;
            case BUILD_TRAINING_MANIFEST -> 52;
            case BUILD_TENSOR_DATASET -> 65;
            case TRAIN_NEURAL_MOTION_PRIOR -> 82;
            case GENERATE_OVERLAY_JSON -> 94;
            case VERIFY_OVERLAY_JSON -> 98;
            case COMPLETE -> 100;
        };
    }

    private String scriptFor(PipelineStep step) {
        return switch (step) {
            case V48_LOAD_GOLD_OVERLAY -> "scripts/v48-01-load-gold-overlay.ps1";
            case V48_GENERATE_SYNTHETIC_VARIANTS -> "scripts/v48-02-generate-quaternion-variants.ps1";
            case V48_BUILD_QUATERNION_DATASET -> "scripts/v48-03-build-quaternion-dataset.ps1";
            case V48_TRAIN_QUATERNION_PRIOR -> "scripts/v48-04-train-quaternion-prior.ps1";
            case V48_GENERATE_IMPORT_JSON -> "scripts/v48-05-generate-import-json.ps1";
            case V48_VALIDATE_IMPORT_JSON -> "scripts/v48-06-validate-import-json.ps1";
            case CLONE_BANDAI_REPO -> "scripts/01-clone-bandai-dataset.ps1";
            case EXTRACT_BANDAI_DATA -> "scripts/02-extract-bandai-data.ps1";
            case SELECT_BANDAI_CLIPS -> "scripts/03-select-bandai-clips.ps1";
            case OFFLINE_RETARGET_TO_MANNY_JSON -> "scripts/06-offline-retarget-bandai-to-manny-json.ps1";
            case CONVERT_BVH_TO_FBX -> "scripts/04-convert-bandai-bvh-to-fbx.ps1";
            case IMPORT_FBX_TO_UNREAL -> "scripts/05-launch-unreal-batch-import.ps1";
            case EXPORT_RETARGETED_ANIMSEQUENCES -> "scripts/06-export-retargeted-animsequences.ps1";
            case BUILD_TRAINING_MANIFEST -> "scripts/07-build-training-manifest.ps1";
            case BUILD_TENSOR_DATASET -> "scripts/08-build-bandai-training-dataset.ps1";
            case TRAIN_NEURAL_MOTION_PRIOR -> "scripts/09-train-bandai-motion-prior.ps1";
            case GENERATE_OVERLAY_JSON -> "scripts/10-generate-bandai-soccer-kick-overlay.ps1";
            case VERIFY_OVERLAY_JSON -> "scripts/11-verify-overlay-json.ps1";
            default -> throw new IllegalArgumentException("No script for " + step);
        };
    }

    private String resolveExportPath(String configPath) {
        if (configPath == null || configPath.isBlank()) return null;
        try {
            File cfgFile = new File(configPath);
            if (!cfgFile.isAbsolute()) {
                File relativeToWorker = cfgFile.getAbsoluteFile();
                File relativeToPipelineRoot = new File(workerProperties.resolvePipelineRootFile(), configPath).getAbsoluteFile();
                cfgFile = relativeToPipelineRoot.exists() ? relativeToPipelineRoot : relativeToWorker;
            }
            cfgFile = cfgFile.getAbsoluteFile();
            Map<String, Object> cfg = objectMapper.readValue(cfgFile, new TypeReference<Map<String, Object>>() {});
            Object trainingObj = cfg.get("training");
            if (!(trainingObj instanceof Map<?, ?> training)) return null;
            Object export = training.get("exportOutput");
            if (export == null || String.valueOf(export).isBlank()) return null;
            Path exportPath = Path.of(String.valueOf(export));
            if (!exportPath.isAbsolute()) exportPath = cfgFile.getParentFile().toPath().getParent().resolve(exportPath).normalize();
            return exportPath.toString();
        } catch (Exception ignored) {
            return null;
        }
    }
}
