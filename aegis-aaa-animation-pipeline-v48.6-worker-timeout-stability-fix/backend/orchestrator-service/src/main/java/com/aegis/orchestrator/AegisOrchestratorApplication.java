package com.aegis.orchestrator;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;

@SpringBootApplication(scanBasePackages = {"com.aegis.orchestrator"})
public class AegisOrchestratorApplication {
    public static void main(String[] args) {
        SpringApplication.run(AegisOrchestratorApplication.class, args);
    }
}
