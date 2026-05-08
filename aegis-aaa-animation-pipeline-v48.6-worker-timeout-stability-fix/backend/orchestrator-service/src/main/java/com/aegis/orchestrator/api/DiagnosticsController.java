package com.aegis.orchestrator.api;

import org.apache.kafka.clients.admin.AdminClient;
import org.apache.kafka.clients.admin.DescribeClusterResult;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.data.redis.connection.RedisConnection;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;

import java.time.Duration;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Properties;
import java.util.concurrent.TimeUnit;

@RestController
public class DiagnosticsController {
    private final StringRedisTemplate redis;
    private final String kafkaBootstrapServers;

    public DiagnosticsController(StringRedisTemplate redis,
                                 @Value("${spring.kafka.bootstrap-servers}") String kafkaBootstrapServers) {
        this.redis = redis;
        this.kafkaBootstrapServers = kafkaBootstrapServers;
    }

    @GetMapping("/api/v1/diagnostics")
    public Map<String, Object> diagnostics() {
        Map<String, Object> out = new LinkedHashMap<>();
        out.put("service", "aegis-orchestrator-service");
        out.put("version", "V47.0");

        Map<String, Object> redisStatus = new LinkedHashMap<>();
        try (RedisConnection connection = redis.getConnectionFactory().getConnection()) {
            redisStatus.put("ok", true);
            redisStatus.put("ping", connection.ping());
        } catch (Exception ex) {
            redisStatus.put("ok", false);
            redisStatus.put("error", ex.getClass().getName());
            redisStatus.put("message", ex.getMessage());
        }
        out.put("redis", redisStatus);

        Map<String, Object> kafkaStatus = new LinkedHashMap<>();
        try {
            Properties props = new Properties();
            props.put("bootstrap.servers", kafkaBootstrapServers);
            props.put("request.timeout.ms", "2500");
            props.put("default.api.timeout.ms", "2500");
            try (AdminClient admin = AdminClient.create(props)) {
                DescribeClusterResult cluster = admin.describeCluster();
                kafkaStatus.put("ok", true);
                kafkaStatus.put("clusterId", cluster.clusterId().get(2500, TimeUnit.MILLISECONDS));
                kafkaStatus.put("nodeCount", cluster.nodes().get(2500, TimeUnit.MILLISECONDS).size());
            }
        } catch (Exception ex) {
            kafkaStatus.put("ok", false);
            kafkaStatus.put("bootstrapServers", kafkaBootstrapServers);
            kafkaStatus.put("error", ex.getClass().getName());
            kafkaStatus.put("message", ex.getMessage());
        }
        out.put("kafka", kafkaStatus);

        return out;
    }
}
