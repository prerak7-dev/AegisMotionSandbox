package com.aegis.common.events;

import com.aegis.common.model.PipelineStep;

import java.time.Instant;
import java.util.Map;

public record PipelineCommand(
        String jobId,
        PipelineStep step,
        String configPath,
        Map<String, Object> options,
        Instant createdAt
) {}
