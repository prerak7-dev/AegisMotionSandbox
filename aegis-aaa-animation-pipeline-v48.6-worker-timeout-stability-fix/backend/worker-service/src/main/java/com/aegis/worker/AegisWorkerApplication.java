package com.aegis.worker;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;

@SpringBootApplication(scanBasePackages = {"com.aegis.worker"})
public class AegisWorkerApplication {
    public static void main(String[] args) {
        SpringApplication.run(AegisWorkerApplication.class, args);
    }
}
