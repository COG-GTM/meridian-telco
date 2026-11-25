package com.meridian.contracts.testkit;

import com.fasterxml.jackson.core.JsonFactory;
import com.fasterxml.jackson.core.JsonParser;
import com.fasterxml.jackson.core.JsonToken;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Token-level JSON comparison for frozen contracts. Two documents match when they have the same
 * keys in the same order, the same string values and the same number literals as written
 * ({@code 640.00} does not match {@code 640.0} or {@code 640}). Whitespace is ignored.
 */
public final class ContractJson {

  private static final JsonFactory FACTORY = new JsonFactory();

  private ContractJson() {}

  /** Returns null when the documents match, otherwise a description of the first difference. */
  public static String firstDifference(String expected, String actual) {
    List<String> want = tokens(expected);
    List<String> got = tokens(actual);
    int n = Math.min(want.size(), got.size());
    for (int i = 0; i < n; i++) {
      if (!want.get(i).equals(got.get(i))) {
        return "token " + i + ": expected " + want.get(i) + " but was " + got.get(i)
            + context(want, i);
      }
    }
    if (want.size() != got.size()) {
      return "expected " + want.size() + " tokens but was " + got.size();
    }
    return null;
  }

  public static void assertMatches(String expected, String actual) {
    String diff = firstDifference(expected, actual);
    if (diff != null) {
      throw new AssertionError("JSON does not match the frozen contract: " + diff);
    }
  }

  private static String context(List<String> tokens, int at) {
    int from = Math.max(0, at - 6);
    return " (after " + String.join(" ", tokens.subList(from, at)) + ")";
  }

  static List<String> tokens(String json) {
    List<String> out = new ArrayList<>();
    try (JsonParser p = FACTORY.createParser(json)) {
      JsonToken t;
      while ((t = p.nextToken()) != null) {
        out.add(switch (t) {
          case FIELD_NAME -> "key:" + p.currentName();
          case VALUE_STRING -> "str:" + p.getText();
          case VALUE_NUMBER_INT, VALUE_NUMBER_FLOAT -> "num:" + p.getText();
          case VALUE_TRUE, VALUE_FALSE, VALUE_NULL -> "lit:" + p.getText();
          default -> t.asString();
        });
      }
    } catch (IOException e) {
      throw new UncheckedIOException("not valid JSON: " + e.getMessage(), e);
    }
    return out;
  }
}
