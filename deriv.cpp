#include <vector>
#include <iostream>
#include <cstdio>
#include <memory>
#include <cctype>
#include <string>
#include <algorithm>

enum lex_type_t { PLUS, MULT, NUMBER, OPEN, CLOSE, END, ID, MINUS };

namespace lexer {
	int cur_lex_line{ 0 };
	int cur_lex_pos{ 0 };
	int cur_lex_end_pos{ 0 };
	enum lex_type_t cur_lex_type;
	std::string cur_lex_text;

	int c;
	void init()
	{
		c = std::cin.get();
	}

	void next()
	{
		cur_lex_text.clear();
		cur_lex_pos = cur_lex_end_pos;
		enum state_t { H, N, P, M, O, C, I, OK, MIN } state = H;
		while (state != OK) {
			switch (state) {
			case H:

				if (c == '+') {
					state = P;
				}
				else if (c == '-') {
					state = MIN;
				}
				else if (c == '*') {
					state = M;
				}
				else if (c == '(') {
					state = O;
				}
				else if (c == ')') {
					state = C;
				}
				else if (std::isdigit(c)) {
					state = N;
				}
				else if (c == '\n') {
					cur_lex_type = END;
					state = OK;
				}
				else if (std::isspace(c)) {
					// stay in H
				}
				else if (std::isalpha(c)) {
					state = I;
				}
				else {
					throw std::logic_error(
						std::to_string(cur_lex_line) + ": " +
						std::to_string(cur_lex_end_pos) + ": "
						"unexpected character with code " + std::to_string(c));
				}
				break;

			case N:
				if (std::isdigit(c)) {
					// stay in N
				}
				else {
					cur_lex_type = NUMBER;
					state = OK;
				}
				break;

			case I:
				if (std::isalnum(c))
				{
					// stay in I
				}
				else
				{
					cur_lex_type = ID;
					state = OK;
				}
				break;

			case P:
				cur_lex_type = PLUS;
				state = OK;
				break;

			case MIN:
				cur_lex_type = MINUS;
				state = OK;
				break;

			case M:
				cur_lex_type = MULT;
				state = OK;
				break;

			case O:
				cur_lex_type = OPEN;
				state = OK;
				break;

			case C:
				cur_lex_type = CLOSE;
				state = OK;
				break;

			case OK:
				break;
			}

			if (state != OK) {
				if (c == '\n') {
					++cur_lex_line;
					cur_lex_pos = cur_lex_end_pos = 0;
				}
				else if (std::isspace(c)) {
					++cur_lex_pos;
					++cur_lex_end_pos;
				}
				else {
					++cur_lex_end_pos;
				}

				if (!std::isspace(c) && cur_lex_type != END) {
					cur_lex_text.push_back(c);
				}

				c = std::cin.get();
			}
		}

	}
}

namespace poliz
{
	class ItemVisitor;

	class ExpressionItem
	{
	public:
		virtual void accept(ItemVisitor&) const = 0;
		virtual ~ExpressionItem() {}
	};

	typedef std::vector<std::shared_ptr<ExpressionItem>> Poliz;

	class IdentifierItem : public ExpressionItem
	{
	private:
		std::string _name;
	public:
		IdentifierItem(std::string name) noexcept : _name(std::move(name)) {}

		const std::string& name() const noexcept { return _name; }

		void accept(ItemVisitor&) const override;
	};

	class NumberItem : public ExpressionItem
	{
	private:
		int _value;
	public:
		NumberItem(int value) noexcept : _value(value) {}

		int value() const noexcept { return _value; }

		void accept(ItemVisitor&) const override;
	};

	class AdditionItem : public ExpressionItem
	{
	private:
		int _arity;
		std::vector<bool> bit_mask;
	public:
		AdditionItem(int arity, std::vector<bool>&& vec) noexcept : _arity(arity), bit_mask(std::move(vec)) {}
		int arity() const noexcept { return _arity; }
		const std::vector<bool>& bit() const noexcept
		{
			return bit_mask;
		}
		void accept(ItemVisitor&) const override;
	};

	class MultiplicationItem : public ExpressionItem
	{
	private:
		int _arity;
	public:
		MultiplicationItem(int arity) noexcept : _arity(arity) {}
		int arity() const noexcept { return _arity; }
		void accept(ItemVisitor&) const override;
	};

	class ItemVisitor
	{
	public:
		virtual ~ItemVisitor() {}
		virtual void visitIdentifier(const IdentifierItem&) = 0;
		virtual void visitAddition(const AdditionItem&) = 0;
		virtual void visitMultiplication(const MultiplicationItem&) = 0;
		virtual void visitNumber(const NumberItem&) = 0;
	};

	void IdentifierItem::accept(ItemVisitor& v) const { v.visitIdentifier(*this); }
	void NumberItem::accept(ItemVisitor& v) const { v.visitNumber(*this); }
	void AdditionItem::accept(ItemVisitor& v) const { v.visitAddition(*this); }
	void MultiplicationItem::accept(ItemVisitor& v) const { v.visitMultiplication(*this); }
}

namespace parser
{
	/*****
GRAMMAR:
addition -> multiplication { [PLUS | MINUS] multiplication }
multiplication -> factor { MULT factor }
factor -> x | NUMBER | OPEN addition CLOSE
	*******/

	// poliz owns its items objects
	poliz::Poliz _poliz;

	// each function push_backs its subexpression poliz onto the parser::poliz
	void _addition();
	void _multiplication();
	void _factor();

	// caller owns the poliz items
	poliz::Poliz parse_full_stdin()
	{
		_poliz.clear();
		lexer::init();
		lexer::next();
		_addition();
		if (lexer::cur_lex_type != END) {
			throw std::logic_error("Excess text after the expression");
		}
		return std::move(_poliz);
	}

	void _addition()
	{
		int arity = 1;
		std::vector<bool> bit_mask;
		_multiplication();
		while (lexer::cur_lex_type == PLUS || lexer::cur_lex_type == MINUS) {
			++arity;
			if (lexer::cur_lex_type == PLUS)
			{
				bit_mask.push_back(true);
			}
			else
			{
				bit_mask.push_back(false);
			}
			lexer::next();
			_multiplication();
		}
		if (arity > 1) {
			_poliz.push_back(std::make_shared<poliz::AdditionItem>(arity, std::move(bit_mask)));
		}
	}

	void _multiplication()
	{
		int arity = 1;
		_factor();
		while (lexer::cur_lex_type == MULT) {
			++arity;
			lexer::next();
			_factor();
		}
		if (arity > 1) {
			_poliz.push_back(std::make_shared<poliz::MultiplicationItem>(arity));
		}
	}

	void _factor()
	{
		if (lexer::cur_lex_type == ID) {
			std::string id{ lexer::cur_lex_text };
			lexer::next();
			_poliz.push_back(std::make_shared<poliz::IdentifierItem>(std::move(id)));
		}
		else if (lexer::cur_lex_type == NUMBER) {
			int value{ std::stoi(lexer::cur_lex_text) };
			lexer::next();
			_poliz.push_back(std::make_shared<poliz::NumberItem>(value));
		}
		else if (lexer::cur_lex_type == OPEN) {
			lexer::next();
			_addition();
			if (lexer::cur_lex_type != CLOSE) {
				throw std::logic_error("Unexpected token; closing parenthesis "
					"is expected");
			}
			lexer::next();
		}
		else {
			throw std::logic_error("Unexpected token; identifier or number or "
				"open parenthesis are expected");
		}
	}
}

// prints the postfix representation of the expression
// this class is intended for debugging
class PostfixPrinter : poliz::ItemVisitor
{
	std::string _output;
public:
	std::string print(const poliz::Poliz& p) {
		_output.clear();
		for (auto i : p) {
			i->accept(*this);
			_output += " ";
		}
		return std::move(_output);
	}
private:
	void visitIdentifier(const poliz::IdentifierItem& item) override {
		_output += item.name();
	}
	void visitNumber(const poliz::NumberItem& item) override {
		_output += std::to_string(item.value());
	}
	void visitAddition(const poliz::AdditionItem& item) override {
		_output += "+(" + std::to_string(item.arity()) + ")";
	}
	void visitMultiplication(const poliz::MultiplicationItem& item) override {
		_output += "*(" + std::to_string(item.arity()) + ")";
	}
};

template <typename C>
void add_back(C& poliz, const C& other) {
	for (auto i : other) {
		poliz.push_back(i);
	}
}

template <typename C>
void add_back(C& poliz, C&& other) {
	for (auto i : other) {
		poliz.push_back(i);
	}
}

// computes the expression derivative
class Deriver : poliz::ItemVisitor
{
private:
	std::vector<poliz::Poliz> derivatives; // each item owns its poliz items
	std::vector<poliz::Poliz> operands; // each item owns its poliz items
	std::string _var;
public:
	poliz::Poliz derive(const poliz::Poliz& e, std::string var) {
		derivatives.clear();
		operands.clear();
		std::swap(_var, var); // this statement is faster replacement for "_var = var;"
			// "var" is passed by copy for this swapping
		for (auto i : e) {
			i->accept(*this);
		}
		return std::move(derivatives.back());
	}
private:

	// specification of each "visit" function:
	// expects the operands at the top of the stack this.operands
	// expects its derivatives at the top of the stack this.derivatives
	// reads the current operation from the "visit" function argument
	// pops its operands and its derivatives from the stacks
	// pushs the whole expression onto the top of this.operands
	// pushs its derivative onto the top of this.derivations

	void visitIdentifier(const poliz::IdentifierItem& item) {
		operands.push_back({ std::make_shared<poliz::IdentifierItem>(item) });
		derivatives.push_back({ std::make_shared<poliz::NumberItem>(item.name() == _var) });
	}

	void visitNumber(const poliz::NumberItem& item) {
		operands.push_back({ std::make_shared<poliz::NumberItem>(item) });
		derivatives.push_back({ std::make_shared<poliz::NumberItem>(0) });
	}

	void visitAddition(const poliz::AdditionItem& item) {
		// d(u + v) = du + dv

		if (item.arity() == 1) {
			return;
		}

		{
			poliz::Poliz addition_expr;
			for (int i = item.arity(); i > 0; --i) {
				add_back(addition_expr, std::move(*(operands.end() - i)));
			}
			operands.resize(operands.size() - item.arity());
			addition_expr.push_back(std::make_shared<poliz::AdditionItem>(item)); 
			operands.push_back(std::move(addition_expr));
		}

		{
			poliz::Poliz addition_deriv;
			for (int i = item.arity(); i > 0; --i) {
				add_back(addition_deriv, std::move(*(derivatives.end() - i)));
			}
			derivatives.resize(derivatives.size() - item.arity());
			addition_deriv.push_back(std::make_shared<poliz::AdditionItem>(item)); 
			derivatives.push_back(std::move(addition_deriv));
		}
	}

	void visitMultiplication(const poliz::MultiplicationItem& item) {
		// d(u * v * t) = du * v * t + u * dv * t + u * v * dt

		if (item.arity() == 1) {
			return;
		}

		std::vector<poliz::Poliz> item_operands;
		{
			for (int i = item.arity(); i > 0; --i) {
				item_operands.push_back(std::move(*(operands.end() - i)));
			}
			operands.resize(operands.size() - item.arity());
		}

		std::vector<poliz::Poliz> item_d_operands;
		{
			for (int i = item.arity(); i > 0; --i) {
				item_d_operands.push_back(std::move(*(derivatives.end() - i)));
			}
			derivatives.resize(derivatives.size() - item.arity());
		}

		{
			poliz::Poliz multipl_expr;
			for (auto& operand : item_operands) {
				add_back(multipl_expr, operand);
			}
			multipl_expr.push_back(std::make_shared<poliz::MultiplicationItem>(item));
			operands.push_back(std::move(multipl_expr));
		}

		{
			poliz::Poliz multipl_deriv;
			for (int i = 0; i < item.arity(); ++i) {
				for (int j = 0; j < item.arity(); ++j) {
					poliz::Poliz& t = (j == i ? item_d_operands[j] : item_operands[j]);
					add_back(multipl_deriv, t);
				}
				multipl_deriv.push_back(std::make_shared<poliz::MultiplicationItem>(item));
			}
			multipl_deriv.push_back(std::make_shared<poliz::AdditionItem>(item.arity(), std::vector<bool>(item.arity(), true))); ///////////////////////////////////
			derivatives.push_back(std::move(multipl_deriv));
		}
	}
};

// prints the infix representation of the expression
class InfixPrinter : poliz::ItemVisitor
{
private:
	std::vector<int> output;
	std::vector<int> priorities;
	int _x;
	// priorities: 3 = id, num ; 2 = mult ; 1 = add
public:
	InfixPrinter(int x) : _x(x) {}

	std::ostream& print(const poliz::Poliz& p, std::ostream& s) {
		output.clear();
		priorities.clear();
		for (auto i : p) {
			i->accept(*this);
		}
		return s << output.back();
	}

	int calculate(const poliz::Poliz& p)
	{
		output.clear();
		priorities.clear();
		for (auto i : p)
		{
			i->accept(*this);
		}
		return output.back();
	}
private:
	void visitIdentifier(const poliz::IdentifierItem& item) {
		output.push_back(_x);
		priorities.push_back(3);
	}
	void visitNumber(const poliz::NumberItem& item) {
		output.push_back(item.value());
		priorities.push_back(3);
	}
private:
	void printNAryItem(int arity) {
		if (arity == 1) {
			return;
		}

		for (int i = 0; i < arity - 1; ++i) {
			int val1 = output.back();
			output.pop_back();
			int val2 = output.back();
			output.pop_back();
			int val3 = val1 * val2;
			output.push_back(val3);
		}
	}

	void printNAryItem2(int arity, const std::vector<bool>& vec) {
		if (arity == 1) {
			return;
		}
		std::vector<int> vec_tmp;
		for (int i = 0; i < arity; ++i)
		{
			vec_tmp.push_back(output.back());
			output.pop_back();
		}
		int counter = 0;
		for (int i = 0; i < arity - 1; ++i) {
			int val1 = vec_tmp.back();
			vec_tmp.pop_back();
			int val2 = vec_tmp.back();
			vec_tmp.pop_back();
			int val3;
			if (vec[counter++] == true)
			{
				val3 = val1 + val2;
			}
			else
			{
				val3 = val1 - val2;
			}
			vec_tmp.push_back(val3);
		}
		output.push_back(vec_tmp.back());
	}
private:
	void visitAddition(const poliz::AdditionItem& item) {
		printNAryItem2(item.arity(), item.bit());
	}
	void visitMultiplication(const poliz::MultiplicationItem& item) {
		printNAryItem(item.arity());
	}
};

int main()
{
	/*if (argc > 1) {
		freopen(argv[1], "r", stdin);
	}*/

	try {
		auto poliz = parser::parse_full_stdin(); // it owns poliz items
		auto derived = Deriver{}.derive(poliz, "x"); // it owns derived items
		int x;
		std::cin >> x;
		std::cout << InfixPrinter(x).calculate(derived) << std::endl;
	}
	catch (std::exception & e) {
		std::cerr << e.what() << std::endl;
	}
}