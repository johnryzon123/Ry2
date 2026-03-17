#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "json/include/lexer.h"
#include "json/include/parser.h"
#include "json/include/value.h"
#include "lexer.h"
#include "token.h"

using namespace RyLSP;

// --- Documentation Map ---
static std::map<Backend::TokenType, std::string> keywordDocs = {
		{Backend::TokenType::DATA, "```ry\ndata name = value\n```\n---\nDefines a new variable. Ry is dynamically typed, "
															 "but once a variable is assigned a non-null value, its type is locked."},
		{Backend::TokenType::UNLESS,
		 "```ry\nunless (condition) {\n  ...\n}\n```\n---\nExecutes if `condition` is `false`."},
		{Backend::TokenType::DO,
		 "```ry\ndo {\n  ...\n} until (condition)\n```\n---\nExecutes repeatedly until the condition becomes `true`."},
		{Backend::TokenType::UNTIL,
		 "```ry\ndo {\n  ...\n} until (condition)\n```\n---\nCondition for the `do-until` loop."},
		{Backend::TokenType::EACH,
		 "```ry\nforeach data i in 0 to 10 { ... }\n```\n---\nIterates over ranges, lists, or strings."},
		{Backend::TokenType::WHILE, "```ry\nwhile (condition) { ... }\n```\n---\nExecutes while `condition` is `true`."},
		{Backend::TokenType::ATTEMPT, "```ry\nattempt { ... } fail err { ... }\n```\n---\nRy's equivalent of `try`."},
		{Backend::TokenType::FAIL, "```ry\nfail err { ... }\n```\n---\nRy's equivalent of `catch`."},
		{Backend::TokenType::FUNC, "```ry\nfunc name(args) { ... }\n```\n---\nDefines a function."},
		{Backend::TokenType::CLASS, "```ry\nclass Name { ... }\n```\n---\nDefines a class."},
		{Backend::TokenType::CHILDOF, "```ry\nclass A childof B\n```\n---\nInheritance (extends)."},
		{Backend::TokenType::PARENT, "```ry\nparent.method()\n```\n---\nAccess superclass methods."},
		{Backend::TokenType::STATIC, "```ry\nstatic func ...\n```\n---\nClass-level method."},
		{Backend::TokenType::PRIVATE, "```ry\nprivate field = 1\n```\n---\nEncapsulation."},
		{Backend::TokenType::STOP, "Terminates a loop (break)."},
		{Backend::TokenType::SKIP, "Skips to next iteration (continue)."},
		{Backend::TokenType::TRUE, "Boolean true."},
		{Backend::TokenType::FALSE, "Boolean false."},
		{Backend::TokenType::NULL_TOKEN, "Null value."},
		{Backend::TokenType::THIS, "Refers to the current instance."},
		{Backend::TokenType::RETURN, "Exits function with value."},
};

static std::map<Backend::TokenType, std::string> keywordLabels = {
		{Backend::TokenType::DATA, "data"},				{Backend::TokenType::UNLESS, "unless"},
		{Backend::TokenType::DO, "do"},						{Backend::TokenType::UNTIL, "until"},
		{Backend::TokenType::EACH, "foreach"},		{Backend::TokenType::WHILE, "while"},
		{Backend::TokenType::ATTEMPT, "attempt"}, {Backend::TokenType::FAIL, "fail"},
		{Backend::TokenType::FUNC, "func"},				{Backend::TokenType::CLASS, "class"},
		{Backend::TokenType::CHILDOF, "childof"}, {Backend::TokenType::PARENT, "parent"},
		{Backend::TokenType::STATIC, "static"},		{Backend::TokenType::PRIVATE, "private"},
		{Backend::TokenType::STOP, "stop"},				{Backend::TokenType::SKIP, "skip"},
		{Backend::TokenType::TRUE, "true"},				{Backend::TokenType::FALSE, "false"},
		{Backend::TokenType::NULL_TOKEN, "null"}, {Backend::TokenType::THIS, "this"},
		{Backend::TokenType::RETURN, "return"},
};

struct DocumentCache {
	std::string content;
	std::vector<Backend::Token> tokens;
};

static std::map<std::string, DocumentCache> documentCache;

// --- Helper Functions ---
struct SignatureInfo {
	std::string label;
	std::vector<std::string> parameters;
	std::string documentation;
};

static std::map<std::string, SignatureInfo> standardSignatures = {
		{"out", {"out(value)", {"value"}, "Prints a value to stdout."}},
		{"input", {"input(prompt)", {"prompt"}, "Reads a line from stdin."}},
		{"type", {"type(obj)", {"obj"}, "Returns the type of the object."}},
		{"len", {"len(obj)", {"obj"}, "Returns the length of a list or string."}},
};

void sendResponse(const JsonValue &response) {
	std::string body = stringify(response);
	std::cout << "Content-Length: " << body.length() << "\r\n\r\n" << body << std::flush;
}

std::string uriToPath(const std::string &uri) {
	if (uri.rfind("file://", 0) == 0) {
		std::string path = uri.substr(7);
#ifdef _WIN32
		if (path.length() > 2 && path[0] == '/' && path[2] == ':') {
			path.erase(0, 1);
		}
#endif
		return path;
	}
	return uri;
}

std::string getHoverContent(const Backend::Token &token) {
	auto it = keywordDocs.find(token.type);
	return (it != keywordDocs.end()) ? it->second : "";
}

int readContentLength() {
	std::string line;
	int length = 0;
	while (std::getline(std::cin, line) && !line.empty() && line != "\r") {
		if (line.rfind("Content-Length: ", 0) == 0) {
			try {
				length = std::stoi(line.substr(16));
			} catch (...) {
				length = 0;
			}
		}
	}
	return length;
}

void validateAndSendDiagnostics(const std::string &uri, const std::vector<Backend::Token> &ryTokens) {
	JsonArray diags;

	for (const auto &token: ryTokens) {
		if (token.type == Backend::TokenType::UNKNOWN) {
			JsonObject diagnostic;
			JsonObject range;
			JsonObject start{{"line", JsonValue(double(token.line - 1))}, {"character", JsonValue(double(token.column - 1))}};
			JsonObject end{{"line", JsonValue(double(token.line - 1))},
										 {"character", JsonValue(double(token.column - 1 + token.lexeme.length()))}};

			range["start"] = JsonValue(start);
			range["end"] = JsonValue(end);
			diagnostic["range"] = JsonValue(range);
			diagnostic["severity"] = JsonValue(double(1)); // Error
			diagnostic["message"] = JsonValue("Unknown token: '" + token.lexeme + "'");
			diagnostic["source"] = JsonValue(std::string("Ry-Lexer"));
			diags.push_back(JsonValue(diagnostic));

			// Optimization: Cap the number of diagnostics to prevent freezing on large files with many errors.
			if (diags.size() >= 100)
				break;
		}
	}

	JsonObject params;
	params["uri"] = JsonValue(uri);
	params["diagnostics"] = JsonValue(diags);

	JsonObject notification;
	notification["jsonrpc"] = JsonValue(std::string("2.0"));
	notification["method"] = JsonValue(std::string("textDocument/publishDiagnostics"));
	notification["params"] = JsonValue(params);

	sendResponse(JsonValue(notification));
}

// --- Main Loop ---

int main() {
	std::cerr << "RyLSP: SERVER STARTING..." << std::endl;

	while (true) {
		int length = readContentLength();
		std::cerr << "RyLSP: Received message length: " << length << std::endl; // Heartbeat 2


		if (std::cin.eof() || length <= 0)
			break;

		std::vector<char> buffer(length);
		std::cin.read(buffer.data(), length);
		std::string_view json_raw(buffer.data(), length);

		JsonLexer lexer(json_raw);
		auto tokens = lexer.scanAllTokens();
		JsonParser parser(tokens);
		auto root = parser.parse();

		if (!root.has_value() || !root->isObject())
			continue;

		JsonObject &msg = std::get<JsonObject>(root->data);
		if (msg.find("method") == msg.end())
			continue;

		std::string method = msg["method"].asString();
		std::cerr << "RyLSP: Method Received -> " << method << std::endl;
		if (method == "exit")
			break;

		if (method == "textDocument/didOpen" || method == "textDocument/didChange") {
			std::cerr << "RyLSP: Entering Change/Open Logic" << std::endl;

			JsonObject params = msg["params"].asObject();
			std::string uri;
			std::string code;
			bool shouldUpdate = false;

			if (method == "textDocument/didOpen") {
				const auto &textDocument = params.at("textDocument").asObject();
				uri = textDocument.at("uri").asString();
				code = textDocument.at("text").asString();
				shouldUpdate = true;
			} else {
				uri = params.at("textDocument").asObject().at("uri").asString();
				const auto &contentChanges = params.at("contentChanges").asArray();
				if (!contentChanges.empty()) {
					code = contentChanges[0].asObject().at("text").asString();
					shouldUpdate = true;
				}
			}

			if (!uri.empty() && shouldUpdate) {
				// Store content first so string_views in tokens are valid
				documentCache[uri].content = std::move(code);
				Backend::Lexer ryLexer(documentCache[uri].content);
				documentCache[uri].tokens = ryLexer.scanTokens();

				std::cerr << "RyLSP: Validating URI: " << uri << " (Length: " << documentCache[uri].content.length() << ")"
									<< std::endl;
				validateAndSendDiagnostics(uri, documentCache[uri].tokens);
				std::cerr << "RyLSP: Diagnostics Sent!" << std::endl;
			} else {
				std::cerr << "RyLSP: ERROR - URI was empty!" << std::endl;
			}
		}

		if (method == "textDocument/didClose") {
			std::cerr << "RyLSP: Closing Document" << std::endl;
			JsonObject params = msg["params"].asObject();
			std::string uri = params.at("textDocument").asObject().at("uri").asString();
			documentCache.erase(uri);
			continue;
		}

		if (msg.find("id") == msg.end())
			continue;
		JsonValue id = msg["id"];

		if (method == "initialize") {
			std::cerr << "RyLSP: Handshaking!" << std::endl;
			JsonObject capabilities;
			capabilities["hoverProvider"] = JsonValue(true);
			capabilities["textDocumentSync"] = JsonValue(double(1)); // Full Sync

			JsonObject completionProvider;
			completionProvider["resolveProvider"] = JsonValue(false);
			completionProvider["triggerCharacters"] = JsonValue(JsonArray{JsonValue(".")});
			capabilities["completionProvider"] = JsonValue(completionProvider);
			capabilities["signatureHelpProvider"] =
					JsonValue(JsonObject{{"triggerCharacters", JsonValue(JsonArray{JsonValue("("), JsonValue(",")})}});

			JsonObject result;
			result["capabilities"] = JsonValue(capabilities);
			JsonObject response;
			response["jsonrpc"] = JsonValue(std::string("2.0"));
			response["id"] = id;
			response["result"] = JsonValue(result);
			sendResponse(JsonValue(response));
			std::cerr << "RyLSP: Initialization Response Sent" << std::endl;
		} else if (method == "textDocument/hover") {
			std::cerr << "RyLSP: Hover Request Received" << std::endl;
			JsonObject params = msg["params"].asObject();
			std::string uri = params.at("textDocument").asObject().at("uri").asString();
			JsonObject position = params["position"].asObject();
			int line = (int) position["line"].asNumber();
			int character = (int) position["character"].asNumber();

			std::cerr << "RyLSP: Hovering at L:" << line << " C:" << character << std::endl;

			std::string hoverText;

			if (documentCache.count(uri)) {
				for (const auto &token: documentCache[uri].tokens) {
					if ((token.line - 1) == line && character >= (token.column - 1) &&
							character < (token.column - 1 + token.lexeme.length())) {
						hoverText = getHoverContent(token);
						break;
					}
				}
			}

			JsonObject response;
			response["jsonrpc"] = JsonValue(std::string("2.0"));
			response["id"] = id;

			if (!hoverText.empty()) {
				JsonObject contents;
				contents["kind"] = JsonValue(std::string("markdown"));
				contents["value"] = JsonValue(hoverText);
				response["result"] = JsonValue(JsonObject{{"contents", JsonValue(contents)}});
			} else {
				response["result"] = JsonValue(std::monostate{});
			}
			sendResponse(JsonValue(response));
		} else if (method == "textDocument/completion") {
			std::cerr << "RyLSP: Completion Request" << std::endl;
			JsonObject params = msg["params"].asObject();
			std::string uri = params.at("textDocument").asObject().at("uri").asString();

			JsonArray items;

			// 1. Add Keywords
			for (const auto &[type, label]: keywordLabels) {
				JsonObject item;
				item["label"] = JsonValue(label);
				item["kind"] = JsonValue(14.0); // Keyword
				if (keywordDocs.count(type)) {
					item["documentation"] = JsonValue(
							JsonObject{{"kind", JsonValue(std::string("markdown"))}, {"value", JsonValue(keywordDocs[type])}});
				}
				items.push_back(JsonValue(item));
			}

			// 2. Add Identifiers from the file
			if (documentCache.count(uri)) {
				std::set<std::string> seen;
				for (const auto &token: documentCache[uri].tokens) {
					if (token.type == Backend::TokenType::IDENTIFIER) {
						if (seen.find(token.lexeme) == seen.end()) {
							seen.insert(token.lexeme);
							JsonObject item;
							item["label"] = JsonValue(token.lexeme);
							item["kind"] = JsonValue(6.0); // Variable
							items.push_back(JsonValue(item));
						}
					}
				}
			}

			JsonObject response;
			response["jsonrpc"] = JsonValue(std::string("2.0"));
			response["id"] = id;
			response["result"] = JsonValue(items);
			sendResponse(JsonValue(response));
		} else if (method == "textDocument/signatureHelp") {
			std::cerr << "RyLSP: Signature Help Request" << std::endl;
			JsonObject params = msg["params"].asObject();
			std::string uri = params.at("textDocument").asObject().at("uri").asString();
			JsonObject position = params["position"].asObject();
			int line = (int) position["line"].asNumber();
			int character = (int) position["character"].asNumber();

			JsonArray signatures;
			int activeParameter = 0;

			if (documentCache.count(uri)) {
				const auto &tokens = documentCache[uri].tokens;
				int idx = -1;
				// Find token at or immediately before cursor
				for (int i = 0; i < (int) tokens.size(); ++i) {
					const auto &t = tokens[i];
					if ((t.line - 1) > line || ((t.line - 1) == line && (t.column - 1) >= character)) {
						idx = i > 0 ? i - 1 : 0;
						break;
					}
					idx = i;
				}

				if (idx >= 0) {
					int balance = 0;
					bool found = false;
					int funcNameIdx = -1;

					for (int i = idx; i >= 0; --i) {
						if (tokens[i].type == Backend::TokenType::RPAREN) {
							balance++;
						} else if (tokens[i].type == Backend::TokenType::LPAREN) {
							if (balance > 0) {
								balance--;
							} else {
								funcNameIdx = i - 1;
								found = true;
								break;
							}
						} else if (tokens[i].type == Backend::TokenType::COMMA && balance == 0) {
							activeParameter++;
						}
					}

					if (found && funcNameIdx >= 0) {
						std::string funcName = tokens[funcNameIdx].lexeme;
						// Standard Library Signatures
						if (standardSignatures.count(funcName)) {
							const auto &info = standardSignatures[funcName];
							JsonObject sig;
							sig["label"] = JsonValue(info.label);
							sig["documentation"] = JsonValue(info.documentation);
							JsonArray paramsJson;
							for (const auto &p: info.parameters) {
								paramsJson.push_back(JsonValue(JsonObject{{"label", JsonValue(p)}}));
							}
							sig["parameters"] = JsonValue(paramsJson);
							signatures.push_back(JsonValue(sig));
						}

						// Naive check for user-defined functions in current file
						for (size_t i = 0; i < tokens.size(); ++i) {
							if (tokens[i].type == Backend::TokenType::FUNC && (i + 2) < tokens.size()) {
								if (tokens[i + 1].lexeme == funcName && tokens[i + 2].type == Backend::TokenType::LPAREN) {
									std::string label = "func " + funcName + "(";
									JsonArray paramsJson;
									size_t j = i + 3;
									bool first = true;
									while (j < tokens.size() && tokens[j].type != Backend::TokenType::RPAREN) {
										if (tokens[j].type == Backend::TokenType::IDENTIFIER) {
											if (!first)
												label += ", ";
											label += tokens[j].lexeme;
											paramsJson.push_back(JsonValue(JsonObject{{"label", JsonValue(tokens[j].lexeme)}}));
											first = false;
										}
										j++;
									}
									label += ")";
									JsonObject sig;
									sig["label"] = JsonValue(label);
									sig["parameters"] = JsonValue(paramsJson);
									signatures.push_back(JsonValue(sig));
									break;
								}
							}
						}
					}
				}
			}

			JsonObject result;
			result["signatures"] = JsonValue(signatures);
			result["activeSignature"] = JsonValue(0.0);
			result["activeParameter"] = JsonValue((double) activeParameter);

			JsonObject response;
			response["jsonrpc"] = JsonValue(std::string("2.0"));
			response["id"] = id;
			response["result"] = JsonValue(result);
			sendResponse(JsonValue(response));
		}
	}
	return 0;
}
