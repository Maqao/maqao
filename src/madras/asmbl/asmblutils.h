/*
   Copyright (C) 2004 - 2018 Université de Versailles Saint-Quentin-en-Yvelines (UVSQ)

   This file is part of MAQAO.

  MAQAO is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 3
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * \file asmblutils.h
 * \page asmblutils Encoder
 * \brief This file defines the functions performing the encoding of instructions based on the data generated by MINJAG from the grammar
 * \date 26 sept. 2011
 *
 * \section encode_main Main concept
 * The encoding is done by performing the reverse operations from what the parser does from the grammar:
 * - Semantic actions functions are used to match an input against a given model (which is what this semantic action generates in the parser),
 * and to update the variables tied to a symbol in the associated expression.
 * - The expression associated to the semantic action is used to build the binary expression corresponding to the encoding of the input
 * - The encoding of a symbol is done by invoking a series of reversed semantic actions until a match is made
 *
 * There is a special mechanism for handling the case where the input to the encoder is not a possible encoding of the top-most symbol.
 * In that case, once the input has been successfully encoded into a symbol, further actions are invoked taking as input the partially encoded input,
 * attempting to encode the symbols up to the top-most symbol. This type of actions are called upward actions, as they represent a progression toward
 * the top-most symbol, and the associated encodings upward encodings. Upward encodings rules are only defined for the actions corresponding to the type
 * of data that can be fed in entry of the encoder.
 *
 * \section encoding Encoding an input
 * The encoding is launched from the architecture specific assembler function, which invokes the \ref insnsym_encode function with a special revsymbol_t
 * structure containing the possible reverse semantic action for this input.
 *
 * \subsection encoding_revaction Performing a reverse semantic action
 * Reversed semantic actions are stored in a \ref revaction_t structure (or \ref revactionup_t if we are dealing with an upward action). They are processed
 * using the \ref revaction (resp. \ref revactionup) function.
 *
 * The processing of a reverse action is done through the following steps:
 * -# Attempting to match the input. This is done by invoking the function associated to this reverse action. If the action is used by an upward
 * encoding, the partial coding is also fed to the function.
 *   - If the match failed, the function is exited returning NULL. The input can't be encoding with the expression associated to this action
 * -# If the match succeeded, the binary encoding associated to this action will be built from the expression associated to the semantic actions and
 *   the variables updated by the function:
 *   - For each element in the expression:
 *     - Constant values are directly added to the encoding
 *     - Tokens (terminal) values are added to the encoding from the values updated by the function
 *     - Symbols (nonterminal) are encoded as described in \ref encoding_revsymbol. If the encoding of the symbol returns NULL, the whole action
 *     is considered failed and NULL is returned.
 * -# If the encoding was successful and the action contains an upward encoding rule, it is invoked with the input and its partial encoding. This is never
 * the case if we are already performing an upward action.
 * -# The encoding is returned.
 *
 * \subsection encoding_revsymbol Encoding a symbol
 *
 * Symbol encoding rules are stored in a \ref revsymbol_t structure and processed by the \ref revsymbol function.
 * A rule consists in a list of reverse semantic actions, which correspond to the list of possible expansions for this symbol.
 * The encoding of the symbol is done by attempting to execute the reverse actions one by one until one returns a non NULL value, meaning the input matches with this
 * expansion. The encoding will be considered completed at this point and the binary encoding returned by the action will be returned as the encoding of the symbol.
 * If all actions in the list return a NULL value, the encoding of the symbol has failed, and NULL will be returned.
 *
 * Upward encoding rules are stored in a \ref revsymbolup_t structure and processed by the \ref revsymbolup function. A rule consists in a list of reverse semantic
 * actions, which correspond to the encodings of all the symbols between the current symbol and the top-most symbol.
 * The upward encoding of the symbol is done by executing those reverse actions sequentially, updating each time the partial encoding of the symbol. When the
 * last reverse action is reached, the encoding correspond to the one of the top-most symbol and the encoding is complete.
 *
 * \todo Possible evolution: Instead of having a single path to the top-most symbol, define an array of possible upward encoding rules for the symbol
 * (array of revsymbolup_t) and try to match with each of them until a match is made. We have to cache the partial encoding somewhere before attempting
 * to complete it by the different rules. This could help to address the case of legacy2 prefixes in x86, which is currently hard coded in the assembly macros.
 * Problem: we may have to try all possible upward encoding rules until the "most right one" is found.
 *
 * \subsection encoding_input Main process
 * The encoding is performed by feeding \ref insnsym_encode with a special \ref revsymbol_t structure which contains a restricted list of possible reverse actions
 * for this input. For the assembler, this corresponds to the list of possible encodings for a given opcode.
 * The encoder then proceeds to invoke the reverse actions for each of those possible encodings as it would when encoding a symbol, and returns the result (NULL
 * or otherwise). *
 */

#ifndef ASMBLUTILS_H_
#define ASMBLUTILS_H_
#include "libmcommon.h"

/**
 * \brief Operand coding
 * This structure stores all data about how an operand's parameter is coded
 * inside the coding of the assembly instruction it belongs to
 * */
typedef struct paramcoding_s {
   int64_t value; /**<Value of the parameter in the coding*/
   uint8_t length; /**<Length of the parameter (in bits) in the coding of the instruction*/
} paramcoding_t;

typedef bitvector_t*(*revactionfct_t)(void*); /**<Prototype of a matching/assembling function for a symbol*/
typedef bitvector_t*(*revactionfctup_t)(void*, bitvector_t*); /**<Prototype of a matching/assembling function for a symbol encompassing an already encoded symbol*/

typedef int (*matchfct_t)(void*, void**, paramcoding_t*); /**<Prototype of a matching function for a given reversed semantic action macro*/

typedef struct revaction_s revaction_t;

/**
 * Structure holding the details for assembling a symbol (parameters for \ref revsymbol) or completing an assembled symbol.
 * */
typedef struct revsymbol_s {
   revaction_t** actions; /**<Array of reversed semantic actions representing possible codings for this symbol
    or the succession of actions to perform before reaching the top-most symbol*/
   uint16_t var_id; /**<Identifier of the variable to pass as input to the functions. Set to 0 if this is not an upward symbol and to 1
    if this is for assembling an instruction*/
   /**\warning The values in var_id are set in MINJAG using the macros defined in arch_sym.h. Update this accordingly if MINJAG changes someday*/
   uint16_t n_actions; /**<Size of \c actions*/
} revsymbol_t;

/**
 * Structure holding the details about a defined token (macro symbol) to assembler
 * */
typedef struct revdefine_s {
   bitvector_t* constant; /**<Value expected for the binary coding of the defined token*/
   uint16_t var_id; /**<Identifier of the defined token*/
   uint8_t size; /**<Size in bits of the defined token*/
   uint8_t endian; /**<Endianness of the defined token*/
} revdefine_t;

/**
 * Structure holding the details about a token to assemble
 * \todo Optimise size (use the sizes in fsmutils.h)
 * */
typedef struct revtoken_s {
   uint16_t var_id; /**<Identifier of the token*/
   uint8_t size; /**<Size in bits of the token*/
   uint8_t endian; /**<Endianness of the token*/
} revtoken_t;

/**
 * Possible types for a \ref revsympart_t structure
 * */
enum parttypes {
   REVSYMPART_T_BINARY = 0, /**<Binary coding*/
   REVSYMPART_T_TOKEN, /**<Terminal symbol*/
   REVSYMPART_T_DEFINE, /**<Defined (macro) symbol*/
   REVSYMPART_T_SYMBOL, /**<Symbol*/
   REVSYMPART_T_INPUT /**<Input value (for upward reverse actions)*/
};

/**
 * Structure holding the details about one element in the binary encoding associated to an reversed semantic action
 * */
typedef struct revsympart_s {
   union {
      bitvector_t* constant; /**<Value for the static binary coding*/
      revdefine_t* define; /**<Defined token (macro)*/
      revtoken_t* token; /**<Token to add*/
      revsymbol_t* symbol; /**<Other symbol to encode*/
   } data; /**<Data associated to the element*/
   uint8_t type; /**<Type of the element*/
} revsympart_t;

/**
 * Structure holding the details of a reverse action or upward reverse action
 * */
struct revaction_s {
   matchfct_t matcher_main; /**<Main matcher function (corresponds to the main macro we are reversing)*/
   revsympart_t** revsyms; /**<Array of symbols contained in the binary expression associated to the action*/
   revsymbol_t* revsymup; /**<Action to perform upwards*/
   uint16_t mainvar_id; /**<Identifier of the main variable in the arrays if the action is an upward action. Will be 0 otherwise*/
   uint16_t n_revsyms; /**<Size of \c revsyms*/
};

/**
 * Encodes an input (instruction)
 * \param insnsymbol Structure containing the list of possible encodings for this instruction
 * \param input Pointer to the instruction to encode
 * \param n_vars Size of the array containing variables (nonterminals)
 * \param n_vals Size of the array containing tokens (terminals)
 * */
extern bitvector_t* insnsym_encode(revsymbol_t* insnsymbol, void* input,
      unsigned int n_vars, unsigned int n_vals);

#endif /* ASMBLUTILS_H_ */
