package com.meridian.inventory;

import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.utility.DockerImageName;

/**
 * PostgreSQL 16 for every @SpringBootTest in this module (picked up by component scan). The image
 * can be pointed at a mirror with -Dmeridian.postgres.image.
 */
@Configuration(proxyBeanMethods = false)
public class TestPostgres {

  static final DockerImageName IMAGE = DockerImageName.parse(
      System.getProperty("meridian.postgres.image", "postgres:16-alpine")).asCompatibleSubstituteFor("postgres");

  @Bean
  @ServiceConnection
  PostgreSQLContainer<?> inventoryPostgres() {
    return new PostgreSQLContainer<>(IMAGE).withDatabaseName("inventory");
  }
}
