#include "analyser.h"
#include <algorithm>
#include <vector>
#include <fstream>
#include <climits>
#include <string>
#include <sstream>
#include <iostream>
#include <cstring>

namespace miniplc0 {
	int32_t returnCount = 0;
	std::pair<std::vector<Instruction>, std::optional<CompilationError>> Analyser::Analyse(std::ostream& output, bool isBinary) {
		auto err = analyseProgram();
		if (err.has_value())
			return std::make_pair(std::vector<Instruction>(), err);
		else {
			printTextFile(output);
			return std::make_pair(_instructions, std::optional<CompilationError>());
		}
			
	}

	// <C0-program> ::= {<variable - declaration>} {<function - definition>}
	std::optional<CompilationError> Analyser::analyseProgram() {
		// 示例函数，示例如何调用子程序
		//<variable - declaration>
			auto err = analysevariable_declaration();
			if (err.has_value())
				return err;

			isGlobal = false;

		//<function - definition>
			err = function_definition();
			if (err.has_value())
				return err;
			return {};
	}

	//<variable - declaration> :: =
	//		[<const - qualifier>]<type - specifier> < init - declarator - list>';'
	//< init - declarator - list > :: =
	//		<init - declarator>{ ',' < init - declarator > }
	//<init - declarator> :: =
	//		<identifier>[<initializer>]
	//<initializer> :: =
	//		'=' < expression >
	std::optional<CompilationError> Analyser::analysevariable_declaration() {
		while (true) {
			bool isConst = false;
			auto next = nextToken();
			if (!next.has_value())
				return {};
			if (next.value().GetType() != TokenType::CONST && next.value().GetType() != TokenType::INT) {
				unreadToken();
				return {};
			}
			if (next.value().GetType() == TokenType::CONST) {
				isConst = true;
				next = nextToken();
				if (!next.has_value() || next.value().GetType() != TokenType::INT) {
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedInt);
				}
			}

			//< init - declarator - list >
			next = nextToken();
			auto next2 = nextToken();
			if (next.value().GetType() == TokenType::IDENTIFIER && next2.value().GetType() == TokenType::LEFT_BRACKET) {
				unreadToken();
				unreadToken();
				unreadToken();
				if (isConst) {
					unreadToken();
				}
				return {};
			}
			unreadToken();
			if (!next.has_value() || next.value().GetType() != TokenType::IDENTIFIER)
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedIdentifier);
			 next2 = nextToken();
			if (!next2.has_value())
				return {};
			if (next2.value().GetType() == TokenType::EQUAL_SIGN) {
				if (isConst) {
					if (!insertsymbol(next.value().GetValueString(), true, true)) {
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrDuplicateDeclaration);
					}
				}
				else {
					if (!insertsymbol(next.value().GetValueString(), false, true)) {
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrDuplicateDeclaration);
					}
				}
				// '<表达式>'
				auto err = analyseexpression();
				if (err.has_value())
					return err;
			}
			else {
				if (isConst) {
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrConstantNeedValue);
				}
				else {
					if (!insertsymbol(next.value().GetValueString(), false, false)) {
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrDuplicateDeclaration);
					}
					addInstruction(Instruction(ipush, 0));
				}
				unreadToken();
			}
			//{ ',' < init - declarator > }
			while (true) {
				auto next = nextToken();
				if (!next.has_value())
					return {};
				if (next.value().GetType() != TokenType::COMMA) {
					unreadToken();
					//return {};
					break;
				}
				next = nextToken();
				if (!next.has_value() || next.value().GetType() != TokenType::IDENTIFIER)
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedIdentifier);
				auto next2 = nextToken();
				if (!next2.has_value())
					return {};
				if (next2.value().GetType() == TokenType::EQUAL_SIGN) {
					// '<表达式>'
					auto err = analyseexpression();
					if (err.has_value())
						return err;
				}
				else {
					unreadToken();
					return {};
				}
			}
			next = nextToken();
			if (!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);
		}
	}

	/*<function - definition> :: =
		<type - specifier><identifier><parameter - clause><compound - statement>*/
	std::optional<CompilationError> Analyser::function_definition() {
		while (true) {
			//void 0,int 1
			int type = -1;
			returnCount = 0;
			local_symbles.clear();
			auto next = nextToken();
			if (!next.has_value())
				return {};
			if (next.value().GetType() != TokenType::VOID && next.value().GetType() != TokenType::INT) {
				unreadToken();
				//是否要报错存疑
				return {};
			}
			if (next.value().GetType() == TokenType::VOID) {
				type = 0;
			}
			if (next.value().GetType() == TokenType::INT) {
				type = 1;
			}

			next = nextToken();
			if (!next.has_value())
				return {};
			if (next.value().GetType() != TokenType::IDENTIFIER) {
				unreadToken();
				//是否要报错存疑
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);
			}
			Token tk = next.value();
			if (type == 0) {
				_void_func.push_back(next.value().GetValueString());
			}
			auto err = analyseparameter_clause(tk);
			if (err.has_value())
				return err;

			// <语句序列>
			err = analysecompound_statement();
			if (err.has_value())
				return err;

			if (type == 0 && returnCount > 0) {
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrReturn);
			}
			if (type == 1 && returnCount == 0) {
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrReturn);
			}
			addInstruction(Instruction(ret));
		}
		return{};
	}

	/*<parameter - clause> :: =
		'('[<parameter - declaration - list>] ')'
	< parameter - declaration - list > :: =
		<parameter - declaration>{ ',' < parameter - declaration > }
	<parameter - declaration> :: =
		[<const - qualifier>]<type - specifier><identifier>*/
	std::optional<CompilationError> Analyser::analyseparameter_clause(Token& tk) {
		int inserted = 0;
		bool isConst = false;
		int32_t Param_num = 1;
		auto next = nextToken();
		if (!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET)
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errparameter_clause);
		next = nextToken();
		if (!next.has_value())
				return {};
		if (next.value().GetType() == TokenType::CONST || next.value().GetType() == TokenType::INT) {
			if (next.value().GetType() == TokenType::CONST) {
				isConst = true;
				next = nextToken();
				if (!next.has_value() || next.value().GetType() != TokenType::INT) {
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedInt);
				}
			}
			next = nextToken();
			if (!next.has_value() || next.value().GetType() != TokenType::IDENTIFIER) {
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errparameter_clause);
			}
			insertsymbol(next.value().GetValueString(), isConst, true);
			while (true) {
				auto next = nextToken();
				if (!next.has_value())
					return {};
				if (next.value().GetType() != TokenType::COMMA) {
					unreadToken();
					//break;
					next = nextToken();
					if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET) {
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errparameter_clause);
					}
					if (!insertfunction(tk, Param_num)) {
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrDuplicateDeclaration);
					}
					return {};
				}
				isConst = false;
				Param_num++;
				next = nextToken();
				if (!next.has_value() || (next.value().GetType() != TokenType::CONST && next.value().GetType() != TokenType::INT)) {
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errparameter_clause);
				}
				if (next.value().GetType() == TokenType::CONST) {
					isConst = true;
					next = nextToken();
					if (!next.has_value() || next.value().GetType() != TokenType::INT) {
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedInt);
					}
				}
				next = nextToken();
				if (!next.has_value() || next.value().GetType() != TokenType::IDENTIFIER) {
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errparameter_clause);
				}
				insertsymbol(next.value().GetValueString(), isConst, true);
			}
		}
		else {
			if (!insertfunction(tk, 0)) {
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrDuplicateDeclaration);
			}
			inserted = 1;
		}
		if(!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET){
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errparameter_clause);
		}
		if (inserted == 0) {
			if (!insertfunction(tk, Param_num)) {
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrDuplicateDeclaration);
			}
		}

		return {};
	}

	/*<compound - statement> :: =
		'{' {<variable - declaration>} < statement - seq> '}'
	< statement - seq > :: =
		{ <statement> }*/
	std::optional<CompilationError> Analyser::analysecompound_statement(){
		auto next = nextToken();
		if (!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACE)
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errcompound_statement);
		auto err = analysevariable_declaration();
		if (err.has_value())
			return err;

		err = analysestatement_seq();
		if (err.has_value())
			return err;

		next = nextToken();
		if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACE)
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errcompound_statement);
		return {};
	}

	std::optional<CompilationError> Analyser::analysestatement_seq() {
		while (true) {
			auto next = nextToken();
			if (!next.has_value())
				return {};
			unreadToken();
			if (next.value().GetType() != TokenType::LEFT_BRACE &&
				next.value().GetType() != TokenType::IF &&
				next.value().GetType() != TokenType::WHILE &&
				next.value().GetType() != TokenType::RETURN &&
				next.value().GetType() != TokenType::SCAN &&
				next.value().GetType() != TokenType::PRINT &&
				next.value().GetType() != TokenType::IDENTIFIER &&
				next.value().GetType() != TokenType::SEMICOLON) {
				return {};
			}
			auto err = analysestatement();
			if (err.has_value())
				return err;
		}
	}

	std::optional<CompilationError> Analyser::analysestatement() {
			auto next = nextToken();
			if (!next.has_value())
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errstatement_seq);
			if (next.value().GetType() == TokenType::LEFT_BRACE) {
				auto err = analysestatement_seq();
				if (err.has_value())
					return err;
				next = nextToken();
				if(!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACE)
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errstatement_seq);
			}

			/*<condition - statement> :: =
				'if' '(' < condition > ')' < statement > ['else' < statement > ]*/
			else if (next.value().GetType() == TokenType::IF) {
				next = nextToken();
				if (!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET)
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errstatement_seq);
				auto err = analysecondition();
				if (err.has_value())
					return err;
				next = nextToken();
				if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errstatement_seq);

				err = analysestatement();
				if (err.has_value())
					return err;

				next = nextToken();
				if (!next.has_value() || next.value().GetType() != TokenType::ELSE) {
					unreadToken();
					std::cout << _functions.size() << std::endl;
					_functions.back().instructions[popJump()].SetX(_functions.back().instructions.size());
					addInstruction(Instruction(nop));
					return {};
				}
				else {
					_functions.back().instructions[popJump()].SetX(_functions.back().instructions.size() + 1);
					addInstruction(Instruction(jmp));
					int32_t offset = _functions.back().instructions.size() - 1;
					auto err = analysestatement();
					if (err.has_value())
						return err;
					_functions.back().instructions[offset].SetX(_functions.back().instructions.size());
				}
				addInstruction(Instruction(nop));
			}

			//<loop - statement> :: =
			//	'while' '(' < condition > ')' < statement >
			else if (next.value().GetType() == TokenType::WHILE) {
				next = nextToken();
				if (!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET)
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedLEFT_BRACKET);
				pushJump(_functions.back().instructions.size());
				auto err = analysecondition();
				if (err.has_value())
					return err;
				next = nextToken();
				if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedRIGHT_BRACKET);

				err = analysestatement();
				if (err.has_value())
					return err;

				_functions.back().instructions[popJump()].SetX(_functions.back().instructions.size() + 1);
				addInstruction(Instruction(jmp, popJump()));
				addInstruction(Instruction(nop));

			}

			//<jump - statement> :: =
			//	<return-statement>
			//<return-statement> :: =
			//	'return'[<expression>] ';'
			else if (next.value().GetType() == TokenType::RETURN) {
				auto next = nextToken();
				if (!next.has_value())
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoEnd);
				if (next.value().GetType() != TokenType::SEMICOLON) {
					unreadToken();
					auto err = analyseexpression();
					if (err.has_value())
						return err;
					next = nextToken();
					if (!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);
					addInstruction(Instruction(iret));
					returnCount++;
				}
				else {
					addInstruction(Instruction(ret));
				}
			}

			//<scan - statement> :: =
			//	'scan' '(' < identifier > ')' ';'
			else if (next.value().GetType() == TokenType::SCAN) {
				auto next = nextToken();
				if (!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET)
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errstatement_seq);
				next = nextToken();
				if (!next.has_value() || next.value().GetType() != TokenType::IDENTIFIER)
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errstatement_seq);
				int32_t offset;
				offset = getOffset(next.value().GetValueString(), global_or_local(next.value().GetValueString()));
				if (global_or_local(next.value().GetValueString()) == -1) {
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclared);
				}
				else if (global_or_local(next.value().GetValueString()) == 1) {
					addInstruction(Instruction(loada, 0, offset));
				}
				else {
					addInstruction(Instruction(loada, 1, offset));
				}
				initailize(next.value().GetValueString());
				addInstruction(Instruction(iscan));
				addInstruction(Instruction(istore));
				next = nextToken();
				if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errstatement_seq);
				next = nextToken();
				if (!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errstatement_seq);
			}

			//<print - statement> :: =
			//	'print' '('[<printable - list>] ')' ';'
			//< printable - list > :: =
			//	<printable>{ ',' < printable > }
			//<printable> :: =
			//	<expression>
			else if (next.value().GetType() == TokenType::PRINT) {
				auto next = nextToken();
				if (!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET)
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errstatement_seq);
				next = nextToken();
				if (!next.has_value())
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoEnd);
				if (next.value().GetType() != TokenType::RIGHT_BRACKET) {
					unreadToken();
					auto err = analyseexpression();
					if (err.has_value())
						return err;
					addInstruction(Instruction(iprint));
					while (true) {
						next = nextToken();
						if (!next.has_value())
							return {};
						if (next.value().GetType() != TokenType::COMMA) {
							unreadToken();
							break;
						}
						err = analyseexpression();
						if (err.has_value())
							return err;
						addInstruction(Instruction(bipush, 32));
						addInstruction(Instruction(cprint));
						addInstruction(Instruction(iprint));
					}
					next = nextToken();
					if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errstatement_seq);
					next = nextToken();
					if (!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);
					addInstruction(Instruction(printl));
				}
				else {
					addInstruction(Instruction(printl));
					next = nextToken();
					if (!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);
				}
			}

			//<assignment - expression> :: =
			//	<identifier><assignment - operator><expression>
			else if (next.value().GetType() == TokenType::IDENTIFIER) {
				auto tk = next.value();
				auto next = nextToken();
				if (isVoid(next.value())) {
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrvoidExpresion);
				}
				if (!next.has_value())
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoEnd);
				//<assignment - expression> :: =
				//	<identifier><assignment - operator><expression>
				if (next.value().GetType() == TokenType::EQUAL_SIGN) {
					int32_t offset;
					offset = getOffset(tk.GetValueString(), global_or_local(tk.GetValueString()));
					int32_t judge = global_or_local(tk.GetValueString());
					if (judge == 1) {
						addInstruction(Instruction(loada, 0, offset));
					}
					else if (judge == 0) {
						addInstruction(Instruction(loada, 1, offset));
					}
					else if (judge == -1) {
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclared);
					}

					if (isConst(tk.GetValueString())) {
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrAssignToConstant);
					}
					if (!isInitailized(tk.GetValueString())) {
						initailize(tk.GetValueString());
					}
					
					auto err = analyseexpression();
					if (err.has_value())
						return err;
					next = nextToken();
					if (!next.has_value())
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);
					if (next.value().GetType() != TokenType::SEMICOLON) {
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);
					}
					addInstruction(Instruction(istore));
				}

				else if (next.value().GetType() == TokenType::LEFT_BRACKET) {
					int param_num = 0;
					int32_t offset = getFunctionOffset(tk.GetValueString());
					if (offset == -1) {
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclared);
					}
					int32_t num = _functions[offset].param_count;
					next = nextToken();
					if (!next.has_value())
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoEnd);
					if (next.value().GetType() != TokenType::RIGHT_BRACKET) {
						unreadToken();
						auto err = analyseexpression();
						param_num++;
						if (err.has_value())
							return err;
						while (true) {
							next = nextToken();
							if (!next.has_value())
								return {};
							if (next.value().GetType() != TokenType::COMMA) {
								unreadToken();
								break;
							}
							err = analyseexpression();
							param_num++;
							if (err.has_value())
								return err;
						}
						next = nextToken();
						if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
							return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedRIGHT_BRACKET);
					}
					if (param_num != num) {
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrArgsnum);
					}
					addInstruction(Instruction(call, offset));
					next = nextToken();
					if (!next.has_value())
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);
					if (next.value().GetType() != TokenType::SEMICOLON) {
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);
					}

				}
				else {
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errstatement_seq);
				}
			}

			else if (next.value().GetType() == TokenType::SEMICOLON) {
				return {};
			}
			return {};
	}

	std::optional<CompilationError> Analyser::analyseexpression() {
		auto err = analysemultiplicative_expression();
		if (err.has_value())
			return err;
		while (true) {
			auto next = nextToken();
			if (!next.has_value())
				return {};
			auto type = next.value().GetType();
			if (type != TokenType::PLUS_SIGN && type != TokenType::MINUS_SIGN) {
				unreadToken();
				return {};
			}
			auto err = analysemultiplicative_expression();
			if (err.has_value())
				return err;
			if (type == TokenType::PLUS_SIGN) {
				addInstruction(Instruction(iadd));
			}
			else if (type == TokenType::MINUS_SIGN) {
				addInstruction(Instruction(isub));
			}
		}
		return {};
	}

	//<multiplicative - expression> :: =
	//	<unary - expression>{ <multiplicative - operator><unary - expression> }
	std::optional<CompilationError> Analyser::analysemultiplicative_expression() {
		auto err = analyseunary_expression();
		if (err.has_value())
			return err;
		while (true) {
			auto next = nextToken();
			if (!next.has_value())
				return {};
			auto type = next.value().GetType();
			if (type != TokenType::MULTIPLICATION_SIGN && type != TokenType::DIVISION_SIGN) {
				unreadToken();
				return {};
			}
			auto err = analyseunary_expression();
			if (err.has_value())
				return err;
			if (type == TokenType::MULTIPLICATION_SIGN)
				addInstruction(Instruction(imul));
			else if (type == TokenType::DIVISION_SIGN)
				addInstruction(Instruction(idiv));
		}
		return {};
	}

	//<unary - expression> :: =
	//	[<unary - operator>]<primary - expression>
	std::optional<CompilationError> Analyser::analyseunary_expression() {
		bool sign = true;
		auto next = nextToken();
		if (next.value().GetType() == MINUS_SIGN)
			sign = false;
		else if (next.value().GetType() == PLUS_SIGN)
			sign = true;
		else
			unreadToken();

		next = nextToken();
		if (!next.has_value())
			return {};
		if (next.value().GetType() == TokenType::LEFT_BRACKET) {
			auto err = analyseexpression();
			if (err.has_value())
				return err;
			next = nextToken();
			if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedRIGHT_BRACKET);
		}
		else if (next.value().GetType() == TokenType::UNSIGNED_INTEGER) {
			addInstruction(Instruction(ipush, std::stol(next.value().GetValueString())));
			//return {};
		}
		else if (next.value().GetType() == TokenType::IDENTIFIER) {
			auto tk = next.value();
			next = nextToken();
			if (next.value().GetType() == TokenType::LEFT_BRACKET) {
				/*auto next = nextToken();
				if (!next.has_value())
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoEnd);
				if (!next.has_value())
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoEnd);
				if (next.value().GetType() != TokenType::RIGHT_BRACKET) {
					unreadToken();
					auto err = analyseexpression();
					if (err.has_value())
						return err;
					while (true) {
						next = nextToken();
						if (!next.has_value())
							return {};
						if (next.value().GetType() != TokenType::COMMA) {
							unreadToken();
							return {};
						}
						err = analyseexpression();
						if (err.has_value())
							return err;
					}
					next = nextToken();
					if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errstatement_seq);*/
				int param_num = 0;
				int32_t offset = getFunctionOffset(tk.GetValueString());
				if (offset == -1) {
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclared);
				}
				int32_t num = _functions[offset].param_count;
				next = nextToken();
				if (!next.has_value())
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoEnd);
				if (next.value().GetType() != TokenType::RIGHT_BRACKET) {
					unreadToken();
					auto err = analyseexpression();
					param_num++;
					if (err.has_value())
						return err;
					while (true) {
						next = nextToken();
						if (!next.has_value())
							return {};
						if (next.value().GetType() != TokenType::COMMA) {
							unreadToken();
							break;
						}
						err = analyseexpression();
						param_num++;
						if (err.has_value())
							return err;
					}
					next = nextToken();
					if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedRIGHT_BRACKET);
					if (param_num != num) {
						return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrArgsnum);
					}
					addInstruction(Instruction(call, offset));
				}
			}
			else {
				unreadToken();
				int32_t offset;
				offset = getOffset(tk.GetValueString(), global_or_local(tk.GetValueString()));
				int temp = global_or_local(tk.GetValueString());
				if (!isInitailized(tk.GetValueString())) {
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotInitialized);
				}
				if (temp == 1) {
					addInstruction(Instruction(loada, 0, offset));
				}
				else if (temp == 0) {
					int32_t x;
					if (isGlobal) {
						x = 0;
					}
					else {
						x = 1;
					}
					addInstruction(Instruction(loada, x, offset));
				}
				else if (temp = -1) {
					return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclared);
				}
				addInstruction(Instruction(iload));
			}
		}
		else {
			return {};
		}
		if (sign == false) {
			addInstruction(Instruction(ineg));
		}
		return {};
	}

	std::optional<CompilationError> Analyser::analysecondition() {
		auto err = analyseexpression();
		if (err.has_value())
			return err;
		auto next = nextToken();
		if (!next.has_value())
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoEnd);

		if (next.value().GetType() == TokenType::LESS) {
			auto err = analyseexpression();
			if (err.has_value())
				return err;
			addInstruction(Instruction(icmp));
			addInstruction(Instruction(jge));
		}
		else if (next.value().GetType() == TokenType::LE) {
			auto err = analyseexpression();
			if (err.has_value())
				return err;
			addInstruction(Instruction(icmp));
			addInstruction(Instruction(jg));
		}
		else if (next.value().GetType() == TokenType::GREATER) {
			auto err = analyseexpression();
			if (err.has_value())
				return err;
			addInstruction(Instruction(icmp));
			addInstruction(Instruction(jle));
		}
		else if (next.value().GetType() == TokenType::GE) {
			auto err = analyseexpression();
			if (err.has_value())
				return err;
			addInstruction(Instruction(icmp));
			addInstruction(Instruction(jl));
		}
		else if (next.value().GetType() == TokenType::NE) {
			auto err = analyseexpression();
			if (err.has_value())
				return err;
			addInstruction(Instruction(icmp));
			addInstruction(Instruction(je));
		}
		else if (next.value().GetType() == TokenType::DE) {
			auto err = analyseexpression();
			if (err.has_value())
				return err;
			addInstruction(Instruction(icmp));
			addInstruction(Instruction(jne));
		}
		else if (next.value().GetType() == TokenType::RIGHT_BRACKET) {
			addInstruction(Instruction(je));
			pushJump(_functions.back().instructions.size() - 1);
			unreadToken();
			return {};
		}
		else {
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::Errcondition);
		}
		pushJump(_functions.back().instructions.size() - 1);
		return {};
	}

	std::optional<Token> Analyser::nextToken() {
		if (_offset == _tokens.size())
			return {};
		// 考虑到 _tokens[0..._offset-1] 已经被分析过了
		// 所以我们选择 _tokens[0..._offset-1] 的 EndPos 作为当前位置
		_current_pos = _tokens[_offset].GetEndPos();
		return _tokens[_offset++];
	}

	void Analyser::unreadToken() {
		if (_offset == 0)
			DieAndPrint("analyser unreads token from the begining.");
		_current_pos = _tokens[_offset - 1].GetEndPos();
		_offset--;
	}

	//寻找符号表中的偏移,0 globle ,1 local
	int32_t Analyser::getOffset(std::string name,int type) {
		if (type == 0) {
			for (int32_t i = 0; i < globle_symbles.size(); i++) {
				if (globle_symbles[i].name == name) {
					return i;
				}
			}
		}
		else if (type == 1) {
			for (int32_t i = 0; i < local_symbles.size(); i++) {
				if (local_symbles[i].name == name) {
					return i;
				}
			}
		}
		return -1;
	}

	//返回函数表的偏移
	int32_t Analyser::getFunctionOffset(std::string name) {
		for (int32_t i = 0; i < _functions.size(); i++) {
			if (name == _functions[i].name) {
				return i;
			}
		}
		return -1;
	}

	//插入符号表
	bool 	Analyser::insertsymbol(std::string name, bool isConst, bool initailized) {
		if (isExists(name)) {
			return false;
		}
		if (isGlobal) {
			symble symblea;
			symblea.name = name;
			symblea.initailized = initailized;
			symblea.isConst = isConst;
			globle_symbles.push_back(symblea);
		}
		else {
			symble symblea;
			symblea.name = name;
			symblea.initailized = initailized;
			symblea.isConst = isConst;
			local_symbles.push_back(symblea);
		}
		return true;
	}

	//检查是否重复定义
	bool Analyser::isExists(std::string name) {
		if (isGlobal) {
			if (getOffset(name, 0) != -1) {
				return true;
			}
		}
		else {
			if (getOffset(name, 1) != -1) {
				return true;
			}
		}
		return false;
	}

	bool Analyser::addInstruction(Instruction instruction) {
		if (isGlobal) {
			_instructions.push_back(instruction);
		}
		else {
			if (_functions.size() <= 0) {
				return false;
			}
			else {
				_functions.back().instructions.push_back(instruction);
			}
		}
		return true;
	}

	//插入函数
	bool Analyser::insertfunction(Token& tk, int32_t param) {
		if (getOffset(tk.GetValueString(), 0) != -1) {
			return false;
		}
		if (isFunc_exists(tk)) {
			return false;
		}
		function Func;
		Func.instructions.clear();
		Func.name = tk.GetValueString();
		Func.param_count = param;
		_functions.push_back(Func);
		return true;
	}
	//函数是否存在
	bool Analyser::isFunc_exists(Token& tk) {
		for (int i = 0; i < _functions.size(); i++) {
			if (_functions[i].name == tk.GetValueString())
				return true;
		}
		return false;
	}

	int32_t Analyser::popJump() {
		int32_t result = _jump_stack.back();
		_jump_stack.pop_back();
		return result;
	}

	void Analyser::pushJump(int32_t jump) {
		_jump_stack.push_back(jump);
	}

	// 全局变量返回0，局部变量返回1
	int32_t Analyser::global_or_local(std::string name) {
		if (!isGlobal) {
			if (getOffset(name,1) != -1) {
				return 1;
			}
		}
		if (getOffset(name,0) != -1) {
			return 0;
		}
		return -1;
	}

	//赋值
	bool Analyser::initailize(std::string name) {
		int32_t offset;
		if (global_or_local(name) == 1) {
			offset = getOffset(name, 1);
			local_symbles[offset].initailized = true;
			return true;
		}
		else if (global_or_local(name) == 0) {
			offset = getOffset(name, 0);
			globle_symbles[offset].initailized = true;
			return true;
		}
		else return false;
	}

	bool Analyser::isConst(std::string name) {
		int32_t offset;
		offset = getOffset(name, global_or_local(name));
		if (global_or_local(name) == 1) {
			return local_symbles[offset].isConst;
		}
		else if (global_or_local(name) == 0) {
			return globle_symbles[offset].isConst;
		}
		else {
			return false;
		}
	}

	bool Analyser::isInitailized(std::string name) {
		int32_t offset;
		offset = getOffset(name, global_or_local(name));
		if (global_or_local(name) == 1) {
			return local_symbles[offset].initailized;
		}
		else if (global_or_local(name) == 0) {
			return globle_symbles[offset].initailized;
		}
		else {
			return false;
		}
	}

	bool Analyser::isVoid(Token& tk) {
		for (int32_t i = 0; i < _void_func.size(); i++) {
			if (tk.GetValueString() == _void_func[i])
				return true;
		}
		return false;
	}

	//输出文本文件
	void Analyser::printTextFile(std::ostream& output) {
		output << ".constants:" << std::endl;
		for (int32_t i = 0; i < _functions.size(); i++) {
			output << i << " " << 'S' << " \"" << _functions[i].name << "\"" << std::endl;
		}
		output << "." << "start" << ":" << std::endl;
		for (int32_t i = 0; i < _instructions.size(); i++) {
			output << i << " ";
			printTextInstruction(_instructions[i], output);
		}
		output << ".functions:" << std::endl;
		for (int32_t i = 0; i < _functions.size(); i++) {
			output << i << " " << i << " " << _functions[i].param_count << " " << "1" << std::endl;
		}
		for (int32_t i = 0; i < _functions.size(); i++) {
			output << "." << "F" << i << ":" << std::endl;
			for (int32_t j = 0; j < _functions[i].instructions.size(); j++) {
				output << j << " ";
				printTextInstruction(_functions[i].instructions[j], output);
			}
		}
	}

	//输出文本指令
	void Analyser::printTextInstruction(Instruction instruction, std::ostream& output) {
		std::string name;
		switch (instruction.GetOperation())
		{
		case miniplc0::bipush: {
			name = "bipush";
			break;
		}
		case miniplc0::ipush: {
			name = "ipush";
			break;
		}
		case miniplc0::pop: {
			name = "pop";
			break;
		}
		case miniplc0::loadc: {
			name = "loadc";
			break;
		}
		case miniplc0::loada: {
			name = "loada";
			//	std::cout << "Y=" << instruction.GetY() << std::endl;
			break;
		}
		case miniplc0::iload: {
			name = "iload";
			break;
		}
		case miniplc0::aload: {
			name = "aload";
			break;
		}
		case miniplc0::iaload: {
			name = "iaload";
			break;
		}
		case miniplc0::istore: {
			name = "istore";
			break;
		}
		case miniplc0::iastore: {
			name = "iastore";
			break;
		}
		case miniplc0::iadd: {
			name = "iadd";
			break;
		}
		case miniplc0::isub: {
			name = "isub";
			break;
		}
		case miniplc0::imul: {
			name = "imul";
			break;
		}
		case miniplc0::idiv: {
			name = "idiv";
			break;
		}
		case miniplc0::ineg: {
			name = "ineg";
			break;
		}
		case miniplc0::icmp: {
			name = "icmp";
			break;
		}
		case miniplc0::jmp: {
			name = "jmp";
			break;
		}
		case miniplc0::je: {
			name = "je";
			break;
		}
		case miniplc0::jne: {
			name = "jne";
			break;
		}
		case miniplc0::jl: {
			name = "jl";
			break;
		}
		case miniplc0::jle: {
			name = "jle";
			break;
		}
		case miniplc0::jg: {
			name = "jg";
			break;
		}
		case miniplc0::jge: {
			name = "jge";
			break;
		}
		case miniplc0::call: {
			name = "call";
			break;
		}
		case miniplc0::ret: {
			name = "ret";
			break;
		}
		case miniplc0::iret: {
			name = "iret";
			break;
		}
		case miniplc0::iprint: {
			name = "iprint";
			break;
		}
		case miniplc0::cprint: {
			name = "cprint";
			break;
		}
		case miniplc0::printl: {
			name = "printl";
			break;
		}
		case miniplc0::iscan: {
			name = "iscan";
			break;
		}
		case miniplc0::nop: {
			name = "nop";
			break;
		}
		default:
			name = "error";
			break;
		}
		output << name;
		if (instruction.GetX() >= 0)
			output << " " << instruction.GetX();
		if (instruction.GetY() >= 0)
			output << ", " << instruction.GetY();
		output << std::endl;
	}
}