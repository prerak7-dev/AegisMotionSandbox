package com.aegis.common.events;

import com.aegis.common.model.JobStatus;
import com.aegis.common.model.PipelineStep;

import java.time.Instant;
import java.util.Map;

public record PipelineEvent(
        String jobId,
        PipelineStep step,
        JobStatus status,
        int progress,
        String message,
        Map<String, Object> details,
        Instant createdAt
) {}
