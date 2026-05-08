package com.aegis.worker.kafka;

import com.aegis.common.events.AegisTopics;
import com.aegis.common.events.PipelineCommand;
import com.aegis.common.model.JobState;
import com.aegis.common.model.JobStatus;
import com.aegis.common.model.PipelineStep;
import com.aegis.worker.service.StepExecutor;
import com.aegis.worker.service.StepPlanner;
import com.aegis.worker.service.WorkerJobStore;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.stereotype.Service;

import java.time.Instant;

@Service
public class PipelineCommandConsumer {
    private final StepExecutor executor;
    private final StepPlanner planner;
    private final WorkerJobStore jobStore;
    private final KafkaTemplate<String, PipelineCommand> kafkaTemplate;

    public PipelineCommandConsumer(StepExecutor executor, StepPlanner planner, WorkerJobStore jobStore,
                                   KafkaTemplate<String, PipelineCommand> kafkaTemplate) {
        this.executor = executor;
        this.planner = planner;
        this.jobStore = jobStore;
        this.kafkaTemplate = kafkaTemplate;
    }

    @KafkaListener(topics = AegisTopics.PIPELINE_COMMANDS, groupId = "${spring.kafka.consumer.group-id:aegis-worker-v47}")
    public void onCommand(PipelineCommand command) {
        try {
            JobState current = jobStore.find(command.jobId()).orElse(null);
            if (current == null) {
                return;
            }
            if (isTerminal(current.getStatus())) {
                return;
            }
            if (current.getCurrentStep() != command.step()) {
                jobStore.appendLog(command.jobId(), "[SKIP_STALE_COMMAND] Ignoring " + command.step()
                        + " because job is currently at " + current.getCurrentStep() + ".");
                return;
            }

            jobStore.appendLog(command.jobId(), "[WORKER_RECEIVED] " + command.step());
            boolean shouldContinue = executor.execute(command);
            if (!shouldContinue) {
                return;
            }

            PipelineStep next = planner.next(command.step(), command.options());
            if (next == PipelineStep.COMPLETE) {
                jobStore.update(command.jobId(), next, JobStatus.COMPLETED, 100, "Pipeline complete");
                return;
            }

            jobStore.update(command.jobId(), next, JobStatus.QUEUED, executor.progressFor(next), "Queued " + next);
            kafkaTemplate.send(AegisTopics.PIPELINE_COMMANDS, command.jobId(),
                    new PipelineCommand(command.jobId(), next, command.configPath(), command.options(), Instant.now()));
        } catch (Exception ex) {
            jobStore.update(command.jobId(), command.step(), JobStatus.FAILED, executor.progressFor(command.step()),
                    "Worker exception: " + ex.getMessage());
        }
    }

    private boolean isTerminal(JobStatus status) {
        return status == JobStatus.COMPLETED || status == JobStatus.FAILED || status == JobStatus.CANCELLED;
    }
}
