package com.aegis.worker.service;

import com.aegis.common.model.PipelineStep;
import org.springframework.stereotype.Service;

import java.util.Map;

@Service
public class StepPlanner {
    public PipelineStep next(PipelineStep step, Map<String, Object> options) {
        if (options == null) options = Map.of();
        boolean skipRetarget = bool(options, "skipRetarget");
        boolean skipConvert = bool(options, "skipConvert");
        boolean skipUnrealImport = bool(options, "skipUnrealImport");
        boolean skipTraining = bool(options, "skipTraining");
        boolean useOfflineRetarget = !options.containsKey("useOfflineRetarget") || bool(options, "useOfflineRetarget");
        boolean pauseForManualRetarget = !options.containsKey("pauseForManualRetarget") || bool(options, "pauseForManualRetarget");

        return switch (step) {
            case V48_LOAD_GOLD_OVERLAY -> PipelineStep.V48_GENERATE_SYNTHETIC_VARIANTS;
            case V48_GENERATE_SYNTHETIC_VARIANTS -> PipelineStep.V48_BUILD_QUATERNION_DATASET;
            case V48_BUILD_QUATERNION_DATASET -> skipTraining ? PipelineStep.V48_GENERATE_IMPORT_JSON : PipelineStep.V48_TRAIN_QUATERNION_PRIOR;
            case V48_TRAIN_QUATERNION_PRIOR -> PipelineStep.V48_GENERATE_IMPORT_JSON;
            case V48_GENERATE_IMPORT_JSON -> PipelineStep.V48_VALIDATE_IMPORT_JSON;
            case V48_VALIDATE_IMPORT_JSON -> PipelineStep.COMPLETE;

            case CLONE_BANDAI_REPO -> PipelineStep.EXTRACT_BANDAI_DATA;
            case EXTRACT_BANDAI_DATA -> PipelineStep.SELECT_BANDAI_CLIPS;
            case SELECT_BANDAI_CLIPS -> {
                if (skipRetarget) yield PipelineStep.BUILD_TRAINING_MANIFEST;
                if (useOfflineRetarget) yield PipelineStep.OFFLINE_RETARGET_TO_MANNY_JSON;
                yield skipConvert ? PipelineStep.IMPORT_FBX_TO_UNREAL : PipelineStep.CONVERT_BVH_TO_FBX;
            }
            case OFFLINE_RETARGET_TO_MANNY_JSON -> PipelineStep.BUILD_TRAINING_MANIFEST;
            case CONVERT_BVH_TO_FBX -> skipUnrealImport ? PipelineStep.EXPORT_RETARGETED_ANIMSEQUENCES : PipelineStep.IMPORT_FBX_TO_UNREAL;
            case IMPORT_FBX_TO_UNREAL -> pauseForManualRetarget ? PipelineStep.WAIT_FOR_MANUAL_RETARGET : PipelineStep.EXPORT_RETARGETED_ANIMSEQUENCES;
            case WAIT_FOR_MANUAL_RETARGET -> PipelineStep.EXPORT_RETARGETED_ANIMSEQUENCES;
            case EXPORT_RETARGETED_ANIMSEQUENCES -> PipelineStep.BUILD_TRAINING_MANIFEST;
            case BUILD_TRAINING_MANIFEST -> PipelineStep.BUILD_TENSOR_DATASET;
            case BUILD_TENSOR_DATASET -> skipTraining ? PipelineStep.GENERATE_OVERLAY_JSON : PipelineStep.TRAIN_NEURAL_MOTION_PRIOR;
            case TRAIN_NEURAL_MOTION_PRIOR -> PipelineStep.GENERATE_OVERLAY_JSON;
            case GENERATE_OVERLAY_JSON -> PipelineStep.VERIFY_OVERLAY_JSON;
            case VERIFY_OVERLAY_JSON -> PipelineStep.COMPLETE;
            case COMPLETE -> PipelineStep.COMPLETE;
        };
    }

    private boolean bool(Map<String, Object> options, String key) {
        Object v = options == null ? null : options.get(key);
        return Boolean.TRUE.equals(v) || "true".equalsIgnoreCase(String.valueOf(v));
    }
}
