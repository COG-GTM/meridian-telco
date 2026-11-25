package com.meridian.inventory.api;

import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import java.io.IOException;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;

/** httpd.h sends Access-Control-Allow-Origin: * on every response; the dashboard calls cross-origin. */
@Component
public class LegacyHeadersFilter extends OncePerRequestFilter {

  @Override
  protected void doFilterInternal(HttpServletRequest req, HttpServletResponse res, FilterChain chain)
      throws ServletException, IOException {
    res.setHeader("Access-Control-Allow-Origin", "*");
    chain.doFilter(req, res);
  }
}
