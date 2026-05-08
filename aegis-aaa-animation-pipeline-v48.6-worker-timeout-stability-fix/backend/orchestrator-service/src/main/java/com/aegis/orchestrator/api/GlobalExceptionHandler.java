package com.aegis.orchestrator.api;

import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.kafka.KafkaException;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;
import org.springframework.web.servlet.resource.NoResourceFoundException;
import org.springframework.data.redis.RedisConnectionFailureException;

import java.time.Instant;
import java.util.LinkedHashMap;
import java.util.Map;

@RestControllerAdvice
public class GlobalExceptionHandler {
    @ExceptionHandler(Exception.class)
    public ResponseEntity<Map<String, Object>> handle(Exception ex) {
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("timestamp", Instant.now().toString());
        body.put("status", 500);
        body.put("error", ex.getClass().getName());
        body.put("message", ex.getMessage());

        Throwable root = ex;
        while (root.getCause() != null) {
            root = root.getCause();
        }
        body.put("rootCause", root.getClass().getName());
        body.put("rootMessage", root.getMessage());

        if (ex instanceof RedisConnectionFailureException || root instanceof java.net.ConnectException) {
            body.put("hint", "Redis or Kafka may not be running. Run .\\scripts\\v46-start-infra.ps1 and verify Docker containers are up.");
        } else if (ex instanceof KafkaException) {
            body.put("hint", "Kafka publish failed. Check Kafka at localhost:9092 and Kafka UI at http://localhost:8099.");
        } else if (ex instanceof NoResourceFoundException) {
            body.put("hint", "This endpoint does not exist. Use GET /api/v1/jobs to list jobs or GET /api/v1/jobs/{jobId} for one job.");
        } else {
            body.put("hint", "Check the orchestrator console logs for the full stack trace.");
        }

        return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(body);
    }
}
