#pragma once

#include <tao/pegtl.hpp>

/*
 * The PEGTL grammar for jimd. Closing rules such as id_close are their own types
 * so that parse() can raise a distinct errc when a must<> on one fails.
 */
namespace jimd::grammar
{
using namespace tao::pegtl;

/**
 * struct escape_char - A character that ``\`` can escape.
 */
struct escape_char : one<'\\', '*', '_', '`', '[', '#', ')', '-', '$'> {};
struct escape : seq<one<'\\'>, escape_char> {};
/**
 * struct invalid_escape - Marks a ``\`` that escapes nothing; raises errc::invalid_escape.
 */
struct invalid_escape : failure {};
struct bad_escape : seq<at<one<'\\'>>, must<invalid_escape>> {};

struct target_close : one<')'> {};
/**
 * struct target_balanced - One level of balanced parentheses inside a target.
 */
struct target_balanced : seq<one<'('>, star<not_at<sor<one<'(', ')'>, eol>>, any>, one<')'>> {};
struct target_text : star<sor<target_balanced, seq<not_at<sor<target_close, eol>>, any>>> {};
/**
 * struct resource_target - The ``(target)`` after a link or image key.
 */
struct resource_target : seq<one<'('>, target_text, must<target_close>> {};

struct id_close : one<']'> {};
struct id_text : star<not_at<sor<id_close, eol>>, any> {};
/**
 * struct resource_id_rule - ``[``, @Tag, a key, then ``]``, such as ``[@key]``.
 */
template <char Tag> struct resource_id_rule : seq<one<'['>, one<Tag>, id_text, must<id_close>> {};

struct image_id : resource_id_rule<'!'> {};
struct reference_id : resource_id_rule<'^'> {};
struct link_id : resource_id_rule<'@'> {};

/**
 * struct image - An image: ``[!key]``, or ``[!alt](target)`` inline.
 */
struct image : seq<image_id, opt<resource_target>> {};
struct reference;
/**
 * struct link - A link: ``[@key]``, or ``[@text](target)`` inline.
 */
struct link : seq<link_id, opt<resource_target>> {};
struct resource : sor<image, reference, link> {};

struct resource_start : seq<one<'['>, one<'@', '!', '^'>> {};
/**
 * struct special - What ends a run of text.
 */
struct special : sor<one<'_', '*', '`', '\\', '$'>, resource_start, eol> {};
/**
 * struct intra_word - An underscore between alphanumerics, kept as text.
 */
struct intra_word : seq<alnum, one<'_'>, at<alnum>> {};
struct text : plus<sor<intra_word, seq<not_at<special>, any>>> {};
/**
 * struct inline_code - Code between backticks, on one line.
 */
struct inline_code : seq<one<'`'>, plus<not_at<sor<one<'`'>, eol>>, any>, one<'`'>> {};
struct inline_math_close : one<'$'> {};
/**
 * struct empty_math - Marks maths that holds only whitespace; raises errc::empty_math.
 */
struct empty_math : failure {};
/**
 * struct inline_math - Maths between dollars, on one line.
 */
struct inline_math
    : sor<seq<at<one<'$'>, star<blank>, one<'$'>>, must<empty_math>>,
          seq<one<'$'>, star<not_at<sor<inline_math_close, eol>>, any>, must<inline_math_close>>> {
};

/**
 * struct emphasis_close - The closing @Delim; raises errc::unclosed_emphasis if missing.
 */
template <char Delim> struct emphasis_close : one<Delim> {};
/**
 * struct empty_emphasis - Marks emphasis with nothing inside; raises errc::empty_emphasis.
 */
struct empty_emphasis : failure {};
/**
 * struct emphasis_rule - Inline content between two @Delim; it may hold @Nested, but not
 * itself directly.
 */
template <char Delim, typename Nested>
struct emphasis_rule
    : sor<seq<at<one<Delim>, one<Delim>>, must<empty_emphasis>>,
          seq<one<Delim>,
              star<sor<escape, bad_escape, Nested, inline_code, inline_math, resource, text>>,
              must<emphasis_close<Delim>>>> {};

struct italic;
struct bold;
/**
 * struct italic - Text between underscores.
 */
struct italic : emphasis_rule<'_', bold> {};
/**
 * struct bold - Text between asterisks.
 */
struct bold : emphasis_rule<'*', italic> {};
struct decoration : sor<italic, bold, inline_code> {};

/**
 * struct nested_footnote - Marks a note inside a note; raises errc::nested_footnote.
 */
struct nested_footnote : failure {};
struct nested_reference : seq<string<'[', '^'>, must<nested_footnote>> {};
/**
 * struct footnote_text - Text inside an inline note, which also stops at ``)``.
 */
struct footnote_text : plus<sor<intra_word, seq<not_at<sor<special, target_close>>, any>>> {};
struct footnote_content : star<sor<escape, bad_escape, nested_reference, decoration, inline_math,
                                   image, link, footnote_text>> {};
struct footnote_target : seq<one<'('>, footnote_content, must<target_close>> {};
/**
 * struct reference - A note: ``[^key]``, or ``[^key](content)`` inline.
 */
struct reference : seq<reference_id, opt<footnote_target>> {};

/**
 * struct inline_item - Anything that can appear in a line of text.
 */
struct inline_item : sor<escape, bad_escape, decoration, inline_math, resource, text> {};

struct blank_line : seq<star<blank>, eol> {};

struct resource_def_content : seq<not_at<blank>, plus<not_at<eol>, any>> {};

/**
 * struct resource_def_rule - A link or image definition line: @Id, ``:``, then the target.
 */
template <typename Id>
struct resource_def_rule : seq<bol, Id, one<':'>, star<blank>, resource_def_content, eolf> {};

struct image_def : resource_def_rule<image_id> {};
struct link_def : resource_def_rule<link_id> {};

/**
 * struct reference_def - A note definition line ``[^key]: content``.
 */
struct reference_def : seq<bol, reference_id, one<':'>, star<blank>, plus<inline_item>, eolf> {};
/**
 * struct resource_def - Any definition line.
 */
struct resource_def : sor<image_def, reference_def, link_def> {};

struct block;

struct header_prefix : seq<bol, plus<one<'#'>>> {};
/**
 * struct header - A heading line: one or more ``#``, a blank, then text.
 */
struct header : seq<header_prefix, plus<blank>, plus<inline_item>, eolf> {};
struct quote_open : seq<bol, one<'>'>, star<blank>, eol> {};
struct quote_close : seq<bol, one<'<'>, star<blank>, eolf> {};
/**
 * struct quote_stray - Marks a quote that stops before its ``<`` line; raises errc::syntax.
 */
struct quote_stray : failure {};
struct quote_end : sor<quote_close, seq<eof, must<quote_close>>, must<quote_stray>> {};
/**
 * struct quote - Blocks between a ``>`` line and a ``<`` line.
 */
struct quote : seq<quote_open, star<sor<blank_line, block>>, quote_end> {};

struct fence : three<'`'> {};
struct code_info_char : seq<not_at<sor<blank, one<'`'>, eol>>, any> {};
struct code_info : seq<code_info_char, star<star<blank>, code_info_char>> {};
struct code_open : seq<bol, fence, star<blank>, opt<code_info>, star<blank>, sor<eol, eof>> {};
struct code_close : seq<bol, fence, star<blank>, sor<eol, eof>> {};
struct code_body : star<not_at<code_close>, any> {};
/**
 * struct block_code - Code between fences of three backticks, with an optional info string.
 */
struct block_code : seq<code_open, code_body, must<code_close>> {};
struct list_item_start : seq<bol, one<'-'>, plus<blank>, not_at<eolf>> {};
struct list_item : seq<list_item_start, must<plus<inline_item>, eolf>> {};
/**
 * struct bullet_list - Consecutive ``- `` lines.
 */
struct bullet_list : plus<list_item> {};
struct math_open : seq<bol, two<'$'>, star<blank>, sor<eol, eof>> {};
struct math_close : seq<bol, two<'$'>, star<blank>, sor<eol, eof>> {};
struct math_body : star<not_at<math_close>, any> {};
struct math_blank_body : seq<star<sor<blank, eol>>, math_close> {};
/**
 * struct block_math - A ``$$`` block holding one equation.
 */
struct block_math : sor<seq<at<math_open, math_blank_body>, must<empty_math>>,
                        seq<math_open, math_body, must<math_close>>> {};
struct aligned_open : seq<bol, three<'$'>, star<blank>, sor<eol, eof>> {};
struct aligned_close : seq<bol, three<'$'>, star<blank>, sor<eol, eof>> {};
struct aligned_body : star<not_at<aligned_close>, any> {};
struct aligned_blank_body : seq<star<sor<blank, eol>>, aligned_close> {};
/**
 * struct block_aligned - A ``$$$`` block of aligned equations.
 */
struct block_aligned : sor<seq<at<aligned_open, aligned_blank_body>, must<empty_math>>,
                           seq<aligned_open, aligned_body, must<aligned_close>>> {};
/**
 * struct block_start - A line that ends a paragraph.
 */
struct block_start : sor<blank_line, code_open, math_open, aligned_open, header, quote_open,
                         quote_close, list_item_start, resource_def> {};
/**
 * struct intra_newline - A line break that continues a paragraph.
 */
struct intra_newline : seq<eol, not_at<block_start>, not_at<eof>> {};
/**
 * struct paragraph - Lines of text up to a blank line or another block.
 */
struct paragraph : seq<not_at<block_start>, list<plus<inline_item>, intra_newline>> {};
/**
 * struct block - Any block.
 */
struct block : sor<block_code, block_math, block_aligned, header, quote, bullet_list, resource_def,
                   paragraph> {};

struct metadata_key : plus<not_at<sor<one<']'>, eol>>, any> {};
struct metadata_value : seq<not_at<eol>, any, star<seq<star<blank>, not_at<eol>, any>>> {};
/**
 * struct metadata_def - A metadata line ``[%key]: value``.
 */
struct metadata_def : seq<bol, string<'[', '%'>, metadata_key, one<']'>, one<':'>, star<blank>,
                          metadata_value, star<blank>, eolf> {};

/**
 * struct doc - A whole document: metadata lines, then blocks; parse() starts here.
 */
struct doc : seq<star<sor<metadata_def, blank_line>>, star<sor<blank_line, block>>, must<eof>> {};
}
