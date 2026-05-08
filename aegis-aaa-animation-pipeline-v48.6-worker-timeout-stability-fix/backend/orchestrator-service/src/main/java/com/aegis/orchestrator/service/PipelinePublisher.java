package com.aegis.orchestrator.service;

import com.aegis.common.events.AegisTopics;
import com.aegis.common.events.PipelineCommand;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.stereotype.Service;

@Service
public class PipelinePublisher {
    private final KafkaTemplate<String, PipelineCommand> kafkaTemplate;

    public PipelinePublisher(KafkaTemplate<String, PipelineCommand> kafkaTemplate) {
        this.kafkaTemplate = kafkaTemplate;
    }

    public void publish(PipelineCommand command) {
        kafkaTemplate.send(AegisTopics.PIPELINE_COMMANDS, command.jobId(), command);
    }
}
