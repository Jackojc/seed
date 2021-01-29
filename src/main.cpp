#include <utility>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <array>
#include <string>
#include <vector>
#include <variant>


namespace seed {
	inline std::string read_file(const std::string& fname) {
		auto filesize = std::filesystem::file_size(fname);
		std::ifstream is(fname, std::ios::binary);

		auto str = std::string(filesize + 1, '\0');
		is.read(str.data(), static_cast<std::streamsize>(filesize));

		return str;
	}
}


namespace seed {
	struct View {
		const char *begin = nullptr;
		int length = 0;

		constexpr View() {}

		constexpr View(const char* const begin_, const char* const end_):
			begin(begin_), length(end_ - begin_) {}

		constexpr View(const char* const begin_, int length_):
			begin(begin_), length(length_) {}
	};

	inline std::ostream& operator<<(std::ostream& os, const View& v) {
		const auto& [vbegin, vlength] = v;
		os.write(vbegin, vlength);
		return os;
	}
}


namespace seed {
	struct Token {
		View view{};
		uint8_t type = 0;

		constexpr Token() {}

		constexpr Token(View view_, uint8_t type_):
			view(view_), type(type_) {}
	};

	constexpr bool operator==(const Token& t, const uint8_t type) {
		return t.type == type;
	}

	constexpr bool operator!=(const Token& t, const uint8_t type) {
		return not(t == type);
	}

	inline std::ostream& operator<<(std::ostream& os, const Token& t) {
		const auto& [view, type] = t;
		return (os << view);
	}
}


namespace seed {
	template <typename... Ts>
	constexpr bool in_group(char c, Ts&&... args) {
		return ((c == args) or ...);
	}

	constexpr bool is_whitespace(char c) {
		return in_group(c, ' ', '\n', '\t', '\v', '\f');
	}
}


namespace seed {
	using node_t = int64_t;

	template <typename... Ts>
	class HomogenousVector: public std::vector<std::variant<Ts...>> {
		using std::vector<std::variant<Ts...>>::vector;

		public:
			template <typename T, typename... Xs>
			node_t add(Xs&&... args) {
				this->emplace_back(std::in_place_type<T>, std::forward<Xs>(args)...);
				return { static_cast<int64_t>(this->size() - 1) };
			}
	};
}


namespace seed {
	struct Position {
		int line = 1, column = 1;
	};


	std::ostream& operator<<(std::ostream& os, const Position& pos) {
		return (os << pos.line << ':' << pos.column);
	}


	Position position(const char* ptr, const char* const end) {
		Position coord;
		auto& [line, column] = coord;

		while (ptr++ != end) {
			if (*ptr == '\n') {
				column = 1;
				line++;
			}

			else {
				column++;
			}
		}

		return coord;
	}
}


namespace seed {
	template <typename... Ts> struct overloaded: Ts... { using Ts::operator()...; };
	template <typename... Ts> overloaded(Ts...) -> overloaded<Ts...>;

	template <typename V, typename... Ts>
	constexpr decltype(auto) visit(V&& variant, Ts&&... args) {
		return std::visit(seed::overloaded{
			std::forward<Ts>(args)...
		}, variant);
	}
}



namespace seed {
	template <typename... Ts>
	[[noreturn]] inline void error(Ts&&... args) {
		([] () -> std::ostream& {
			return (std::cerr << "error: ");
		} () << ... << std::forward<Ts>(args)) << '\n';

		std::exit(1);
	}


	inline std::string tabs(int n) {
		return std::string(n, '\t');
	}


	template <typename... Ts>
	inline std::string strcat(Ts&&... args) {
		std::string buf{sizeof...(Ts), '\0'};

		std::stringstream ss{buf};
		((ss << std::forward<Ts>(args)), ...);

		return ss.str();
	}
}


namespace seed {
	#define TOKENS \
		X(TOKEN_NONE) \
		X(TOKEN_EOF) \
		X(TOKEN_LPAREN) \
		X(TOKEN_RPAREN) \
		X(TOKEN_STRING) \
		X(TOKEN_IDENTIFIER)

	#define X(x) #x,
		const char* to_str[] = { TOKENS };
	#undef X

	#define X(x) x,
		enum { TOKENS };
	#undef X

	#undef TOKENS


	inline seed::Token next_token(const char* const start, const char*& ptr) {
		seed::Token tok{{ptr, 1}, TOKEN_NONE};

		auto& [view, type] = tok;
		auto& [vptr, vlen] = view;

		if (*ptr == '\0') {
			type = TOKEN_EOF;
		}

		else if (*ptr == '(') { type = TOKEN_LPAREN; ++ptr; }
		else if (*ptr == ')') { type = TOKEN_RPAREN; ++ptr; }

		else if ((*ptr == '"' or *ptr == '\'') and *(ptr - 1) != '\\') {
			type = TOKEN_STRING;
			char delim = *ptr;

			do {
				++ptr;
			} while (*ptr != delim and *ptr);

			++ptr;  // skip end quote.

			// remove quotes.
			vptr++;
			vlen = ptr - vptr - 1;
		}

		else if (not seed::is_whitespace(*ptr)) {
			type = TOKEN_IDENTIFIER;

			if (*ptr == '\\') {
				++vptr;
			}

			do {
				++ptr;
			} while (not seed::is_whitespace(*ptr) and not in_group(*ptr, '(', ')'));

			vlen = ptr - vptr;
		}

		else if (seed::is_whitespace(*ptr)) {
			do {
				++ptr;
			} while (seed::is_whitespace(*ptr));

			return next_token(start, ptr);
		}

		else {
			error(seed::position(start, ptr), ": unexpected character `", *ptr, "`(", (int)*ptr, ").");
		}

		return tok;
	}


	class Lexer {
		private:
			const char* const start = nullptr;
			const char* str = nullptr;
			Token lookahead{};


		public:
			Lexer(const char* const str_): start(str_), str{str_} {
				advance();
			}


		public:
			const Token& peek() const {
				return lookahead;
			}

			Token advance() {
				Token tok = peek();
				lookahead = next_token(start, str);
				return tok;
			}

			Position position() const {
				return seed::position(start, str);
			}
	};
}


namespace seed {
	struct Identifer {
		seed::Token tok;

		Identifer(const seed::Token& tok_): tok(tok_) {}
		Identifer(): tok() {}
	};

	struct String {
		seed::Token tok;

		String(const seed::Token& tok_): tok(tok_) {}
		String(): tok() {}
	};

	struct List {
		seed::Token op;
		std::vector<seed::node_t> children;

		List(const seed::Token& op_, const std::vector<seed::node_t>& children_): op(op_), children(children_) {}
		List(): op(), children() {}
	};

	struct Empty {
		Empty() {}
	};

	using AST = seed::HomogenousVector<List, Identifer, String, Empty>;
}


namespace seed {
	inline seed::node_t expr(seed::Lexer& lex, seed::AST& tree) {
		if (lex.advance() != TOKEN_LPAREN)
			error(lex.position(), ": expected `(`.");

		seed::Token op = lex.advance();

		if (op == TOKEN_RPAREN) {
			return tree.add<Empty>();
		}

		else if (op != TOKEN_IDENTIFIER and op != TOKEN_STRING)
			error(lex.position(), ": expected identifer or string.");

		std::vector<seed::node_t> children;

		while (lex.peek() != TOKEN_RPAREN and lex.peek() != TOKEN_EOF) {
			if (lex.peek() == TOKEN_LPAREN) {
				children.emplace_back(expr(lex, tree));
			}

			else if (lex.peek() == TOKEN_IDENTIFIER) {
				children.emplace_back(tree.add<Identifer>(lex.advance()));
			}

			else if (lex.peek() == TOKEN_STRING) {
				children.emplace_back(tree.add<String>(lex.advance()));
			}
		}

		if (lex.advance() != TOKEN_RPAREN)
			error(lex.position(), ": expected `)`.");

		return tree.add<List>(op, children);
	}
}


namespace seed {
	template <typename T>
	void render_nodes(
		const T& variant,
		const seed::AST& tree,
		std::string& str,
		const int indent_size, int parent_id, int& node_counter
	) {
		seed::visit(variant,
			[&] (const List& l) {
				int self_id = node_counter++;
				const auto& [op, children] = l;

				str += tabs(indent_size) + strcat("n", self_id, " [label=\"", op, "\"];\n");

				if (self_id != parent_id) {
					str += tabs(indent_size) + strcat("n", parent_id, " -> n", self_id, ";\n");
				}

				for (const auto& child: children) {
					render_nodes(tree[child], tree, str, indent_size, self_id, node_counter);
					node_counter++;
				}
			},

			[&] (const Identifer& x) {
				int self_id = node_counter++;
				str += tabs(indent_size) + strcat("n", self_id, " [label=\"", x.tok, "\"];\n");

				if (self_id != parent_id) {
					str += tabs(indent_size) + strcat("n", parent_id, " -> n", self_id, ";\n");
				}
			},

			[&] (const String& x) {
				int self_id = node_counter++;
				str += tabs(indent_size) + strcat("n", self_id, " [label=\"", x.tok, "\"];\n");

				if (self_id != parent_id) {
					str += tabs(indent_size) + strcat("n", parent_id, " -> n", self_id, ";\n");
				}
			},

			[&] (const Empty&) {}
		);
	}


	template <typename T>
	void render_cluster(
		const T& variant,
		const seed::AST& tree,
		std::string& str,
		int& node_counter,
		const std::string& title = "subgraph",
		const int indent_size = 0
	) {
		str += tabs(indent_size) + title + " {\n";
			render_nodes(variant, tree, str, indent_size + 1, node_counter, node_counter);
			node_counter++;
		str += tabs(indent_size) + "}\n";
	}


	std::string render(
		const std::vector<seed::node_t>& roots,
		const seed::AST& tree,
		const std::string& title = "digraph",
		const int indent_size = 0
	) {
		int node_counter = 0;
		std::string str;

		str += tabs(indent_size) + title + " {\n";

		int graph_id = 0;
		for (const seed::node_t& n: roots) {
			render_cluster(tree[n], tree, str, node_counter, "subgraph cluster" + std::to_string(graph_id), indent_size + 1);
			graph_id++;
		}

		str += tabs(indent_size) + "}\n";

		return str;
	}
}


namespace seed {
	std::vector<seed::node_t> parse(seed::Lexer& lex, seed::AST& tree) {
		std::vector<seed::node_t> roots;

		while (lex.peek() != seed::TOKEN_EOF) {
			roots.emplace_back(seed::expr(lex, tree));
		}

		return roots;
	}
}


int main(int argc, const char* argv[]) {
	if (argc != 2) {
		std::cerr << "usage: wpp <file>\n";
		return -1;
	}

	const std::string fname = argv[1];

	std::error_code ec;
	if (not std::filesystem::exists(fname, ec)) {
		seed::error("file `", fname, "` does not exist.");
	}

	auto expr = seed::read_file(fname);

	seed::AST tree;
	seed::Lexer lex{expr.c_str()};

	auto roots = seed::parse(lex, tree);
	std::cout << seed::render(roots, tree);

	return 0;
}
