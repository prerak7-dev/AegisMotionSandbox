package com.aegis.common.dto;

public record PipelineRunRequest(
        String configPath,
        String pipelineMode,
        Boolean skipClone,
        Boolean skipRetarget,
        Boolean skipConvert,
        Boolean skipUnrealImport,
        Boolean skipTraining,
        Boolean pauseForManualRetarget,
        Boolean useOfflineRetarget,
        String action,
        String style,
        String kickStyle,
        String dominantLeg,
        String targetFbx,
        Integer maxClips,
        Integer variantCount,
        Double durationSeconds,
        Double intensity,
        Double followThrough,
        Double plantStability,
        Double upperBodyCounterbalance
) {
    public boolean shouldUseV48NoRetarget() { return pipelineMode == null || pipelineMode.isBlank() || pipelineMode.equalsIgnoreCase("V48_QUATERNION_NO_RETARGET"); }
    public boolean shouldSkipClone() { return Boolean.TRUE.equals(skipClone); }
    public boolean shouldSkipRetarget() { return Boolean.TRUE.equals(skipRetarget); }
    public boolean shouldSkipConvert() { return Boolean.TRUE.equals(skipConvert); }
    public boolean shouldSkipUnrealImport() { return Boolean.TRUE.equals(skipUnrealImport); }
    public boolean shouldSkipTraining() { return Boolean.TRUE.equals(skipTraining); }
    public boolean shouldPauseForManualRetarget() { return pauseForManualRetarget == null || pauseForManualRetarget; }
    public boolean shouldUseOfflineRetarget() { return useOfflineRetarget == null || useOfflineRetarget; }
}
