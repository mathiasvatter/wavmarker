//
// Created by Mathias Vatter on 15.05.25.
//

#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <typeindex>
#include <functional>
#include <iostream>
#include <mutex>
#include <utility>
#include <vector>
#include <algorithm>
#include "JSON/ast/JSONValue.h"
#include "../util/Error.h"

class Reflectable; // Vorwärtsdeklaration

struct MemberDescriptor {
	std::string name;
	std::type_index type; // Gibt den C++ Typ des Members an
	std::function<void(Reflectable*, JSONValue*)> setter; // Nimmt das Objekt und den JSON-Wert
	std::function<std::unique_ptr<JSONValue>(const Reflectable*)> getter; // displays the value of the member

	MemberDescriptor(std::string n,
					std::type_index t,
					std::function<void(Reflectable*, JSONValue*)> s,
					std::function<std::unique_ptr<JSONValue>(const Reflectable*)> g)
		: name(std::move(n)), type(t), setter(std::move(s)), getter(std::move(g)) {}
};

class Reflectable {
public:
	virtual ~Reflectable() = default;

	// Diese Methode wird von abgeleiteten Klassen implementiert, um ihre Member-Deskriptoren zurückzugeben.
	// Sie wird normalerweise durch Makros generiert.
	[[nodiscard]] virtual const std::map<std::string, MemberDescriptor>& get_member_descriptors() const {
		static const std::map<std::string, MemberDescriptor> empty_descriptors{};
		return empty_descriptors;
	}

	bool set_member_value(const std::string& member_name, JSONValue* value) {
		const auto& descriptors = get_member_descriptors();
		auto it = descriptors.find(member_name);
		if (it != descriptors.end()) {
			it->second.setter(this, value); // Rufe den Setter auf
			return true;
		}
		std::cerr << "Fehler: Member '" << member_name << "' nicht in Reflection-Deskriptoren gefunden für Typ " << typeid(*this).name() << "." << std::endl;
		return false;
	}

	// Neue Methode zum Abrufen von Member-Werten
	[[nodiscard]] std::unique_ptr<JSONValue> get_member_value(const std::string& memberName) const {
		const auto& descriptors = get_member_descriptors();
		auto it = descriptors.find(memberName);
		if (it != descriptors.end()) {
				return it->second.getter(this);
		}
		std::cerr << "Fehler: Member '" << memberName << "' nicht in Reflection-Deskriptoren gefunden für Typ " << typeid(*this).name() << "." << std::endl;
		return nullptr; // Oder ein JSONNull-Objekt
	}

	[[nodiscard]] std::unique_ptr<JSONValue> to_json() const;
	[[nodiscard]] std::string to_json_string() const;

};

// Dieses Makro wird in der Header-Datei der Klasse im public-Teil platziert.
#define DECLARE_REFLECTABLE() \
public: \
	static const std::map<std::string, MemberDescriptor>& get_descriptors(); \
private: \
	static std::map<std::string, MemberDescriptor> s_member_descriptors; \
	static void InitializeReflection(); \
	/* Hilfsmethode, um sicherzustellen, dass InitializeReflection nur einmal aufgerufen wird */ \
	static std::once_flag s_reflection_initialized_flah; \
public: \
	const std::map<std::string, MemberDescriptor>& get_member_descriptors() const override { \
		std::call_once(s_reflection_initialized_flah, [](){ InitializeReflection(); }); \
		return s_member_descriptors; \
	}

// --- in deiner Reflection.h (vor DEFINE_REFLECTABLE_MEMBERS) ---

// 1) Zähle die Anzahl der __VA_ARGS__ (bis 10)
#define PP_NARG(...) PP_NARG_(__VA_ARGS__, PP_RSEQ())
#define PP_NARG_(...) PP_ARG_N(__VA_ARGS__)
#define PP_ARG_N(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,N,...) N
#define PP_RSEQ() 10,9,8,7,6,5,4,3,2,1,0

// 2) Hilfs-Token-Concat
#define PP_CAT(a,b) PP_CAT_I(a,b)
#define PP_CAT_I(a,b) a##b

// FOR_EACH_N Makros erweitert bis N=10
#define FOR_EACH_1(WHAT, Class, X1) WHAT(Class, X1)
#define FOR_EACH_2(WHAT, Class, X1, X2) WHAT(Class, X1); WHAT(Class, X2)
#define FOR_EACH_3(WHAT, Class, X1, X2, X3) WHAT(Class, X1); WHAT(Class, X2); WHAT(Class, X3)
#define FOR_EACH_4(WHAT, Class, X1, X2, X3, X4) WHAT(Class, X1); WHAT(Class, X2); WHAT(Class, X3); WHAT(Class, X4)
#define FOR_EACH_5(WHAT, Class, X1, X2, X3, X4, X5) WHAT(Class, X1); WHAT(Class, X2); WHAT(Class, X3); WHAT(Class, X4); WHAT(Class, X5)
#define FOR_EACH_6(WHAT, Class, X1, X2, X3, X4, X5, X6) WHAT(Class, X1); WHAT(Class, X2); WHAT(Class, X3); WHAT(Class, X4); WHAT(Class, X5); WHAT(Class, X6)
#define FOR_EACH_7(WHAT, Class, X1, X2, X3, X4, X5, X6, X7) WHAT(Class, X1); WHAT(Class, X2); WHAT(Class, X3); WHAT(Class, X4); WHAT(Class, X5); WHAT(Class, X6); WHAT(Class, X7)
#define FOR_EACH_8(WHAT, Class, X1, X2, X3, X4, X5, X6, X7, X8) WHAT(Class, X1); WHAT(Class, X2); WHAT(Class, X3); WHAT(Class, X4); WHAT(Class, X5); WHAT(Class, X6); WHAT(Class, X7); WHAT(Class, X8)
#define FOR_EACH_9(WHAT, Class, X1, X2, X3, X4, X5, X6, X7, X8, X9) WHAT(Class, X1); WHAT(Class, X2); WHAT(Class, X3); WHAT(Class, X4); WHAT(Class, X5); WHAT(Class, X6); WHAT(Class, X7); WHAT(Class, X8); WHAT(Class, X9)
#define FOR_EACH_10(WHAT, Class, X1, X2, X3, X4, X5, X6, X7, X8, X9, X10) WHAT(Class, X1); WHAT(Class, X2); WHAT(Class, X3); WHAT(Class, X4); WHAT(Class, X5); WHAT(Class, X6); WHAT(Class, X7); WHAT(Class, X8); WHAT(Class, X9); WHAT(Class, X10)
// Bei Bedarf können hier weitere FOR_EACH_N Makros hinzugefügt werden.

// 4) Allgemeines FOR_EACH, wählt automatisch FOR_EACH_n
#define FOR_EACH(WHAT, Class, ...)                         \
	PP_CAT(FOR_EACH_, PP_NARG(__VA_ARGS__))(WHAT, Class, __VA_ARGS__)

// --- danach: dein REGISTER_MEMBER mit dem nullptr-Trick für typeid ---

#undef REGISTER_MEMBER
#define REGISTER_MEMBER(CLASS, MEMBER)                                        \
	CLASS::s_member_descriptors.insert_or_assign(                              \
		#MEMBER,                                                              \
		MemberDescriptor(                                                     \
			#MEMBER,                                                          \
			std::type_index(typeid(std::remove_cvref_t<decltype(std::declval<CLASS>().MEMBER)>)), \
			/* Setter */                                                      \
            [](Reflectable* obj, JSONValue* val) {                            \
                auto* concrete = static_cast<CLASS*>(obj);                    \
                assign_from_json(concrete->MEMBER, val);                      \
            },                                                                 \
            /* Getter */                                                      \
            [](const Reflectable* obj) -> std::unique_ptr<JSONValue> {        \
                const auto* concrete = static_cast<const CLASS*>(obj);        \
                return convert_to_json(concrete->MEMBER);                     \
            }                                                                  \
		)                                                                      \
	)

// --- und zum Schluss: das neue DEFINE_REFLECTABLE_MEMBERS — ganz ohne Lambdas ---

#undef DEFINE_REFLECTABLE_MEMBERS
#define DEFINE_REFLECTABLE_MEMBERS(ClassName, ...)                                \
	inline std::map<std::string, MemberDescriptor> ClassName::s_member_descriptors;       \
	inline std::once_flag ClassName::s_reflection_initialized_flah;                        \
	inline const std::map<std::string, MemberDescriptor>& ClassName::get_descriptors() {  \
		std::call_once(                                                            \
			ClassName::s_reflection_initialized_flah,                               \
			&ClassName::InitializeReflection                                       \
		);                                                                         \
		return ClassName::s_member_descriptors;                                     \
	}                                                                              \
	inline void ClassName::InitializeReflection() {                                       \
		/* für jedes Member im Variadic-Argument: REGISTER_MEMBER(ClassName, X) */\
		FOR_EACH(REGISTER_MEMBER, ClassName, __VA_ARGS__);                         \
	}

/// Helper functions

template<typename T>
std::unique_ptr<JSONValue> convert_to_json(const T& value) {
	if constexpr (std::is_same_v<T, std::string>) {
		return std::make_unique<JSONString>(value);
	} else if constexpr (std::is_integral_v<T>) {
		return std::make_unique<JSONInt>(static_cast<long long>(value));
	} else if constexpr (std::is_enum_v<T>) {
		return std::make_unique<JSONInt>(static_cast<long long>(value));
	} else if constexpr (std::is_base_of_v<Reflectable, T>) {
		auto json_object = std::make_unique<JSONObject>();
		for (const auto& [name, descriptor] : value.get_member_descriptors()) {
			auto json_value = descriptor.getter(&value);
			json_object->add(name, json_value ? std::move(json_value) : std::make_unique<JSONNull>());
		}
		return json_object;
	} else {
		std::cerr << "Warnung: Keine spezifische convert_to_json für Typ " << typeid(T).name() << std::endl;
		return std::make_unique<JSONNull>();
	}
}

inline std::unique_ptr<JSONValue> Reflectable::to_json() const {
	auto json_object = std::make_unique<JSONObject>();
	for (const auto& [name, descriptor] : get_member_descriptors()) {
		auto json_value = descriptor.getter(this);
		json_object->add(name, json_value ? std::move(json_value) : std::make_unique<JSONNull>());
	}
	return json_object;
}

inline std::string Reflectable::to_json_string() const {
	return to_json()->get_string();
}

template<typename T>
std::unique_ptr<JSONValue> convert_to_json(const std::vector<T>& vec) {
	auto json_array = std::make_unique<JSONArray>();
	for (const auto& item : vec) {
		json_array->add(convert_to_json(item));
	}
	return json_array;
}

template<typename T>
std::unique_ptr<JSONValue> convert_to_json(const std::optional<T>& value) {
	if (!value) {
		return std::make_unique<JSONNull>();
	}
	return convert_to_json(*value);
}

template<typename T>
void assign_from_json(T& member, JSONValue* jsonVal) {
	if constexpr (std::is_same_v<T, std::string>) {
		if (auto* sv = dynamic_cast<JSONString*>(jsonVal)) {
			member = sv->value;
		} else {
			std::cerr << "Fehler: Erwartete JSONString für std::string, bekam "
					  << (jsonVal ? typeid(*jsonVal).name() : "null") << std::endl;
		}
	} else if constexpr (std::is_integral_v<T>) {
		if (auto* iv = dynamic_cast<JSONInt*>(jsonVal)) {
			if (iv->value < static_cast<long long>(std::numeric_limits<T>::min()) ||
				iv->value > static_cast<long long>(std::numeric_limits<T>::max())) {
				std::cerr << "Fehler: JSONInt außerhalb des Wertebereichs für Typ " << typeid(T).name() << std::endl;
			} else {
				member = static_cast<T>(iv->value);
			}
		} else if (auto* sv = dynamic_cast<JSONString*>(jsonVal)) {
			try {
				const auto parsed = std::stoll(sv->value);
				if (parsed < static_cast<long long>(std::numeric_limits<T>::min()) ||
					parsed > static_cast<long long>(std::numeric_limits<T>::max())) {
					std::cerr << "Fehler: JSONString außerhalb des Wertebereichs für Typ " << typeid(T).name() << std::endl;
				} else {
					member = static_cast<T>(parsed);
				}
			} catch (const std::exception& e) {
				std::cerr << "Fehler: Konnte JSONString '" << sv->value << "' nicht zu Integer konvertieren: " << e.what() << std::endl;
			}
		} else {
			std::cerr << "Fehler: Erwartete JSONInt für Integer-Typ, bekam "
					  << (jsonVal ? typeid(*jsonVal).name() : "null") << std::endl;
		}
	} else if constexpr (std::is_enum_v<T>) {
		if (auto* iv = dynamic_cast<JSONInt*>(jsonVal)) {
			member = static_cast<T>(iv->value);
		} else {
			std::cerr << "Fehler: Erwartete JSONInt für Enum-Typ, bekam "
					  << (jsonVal ? typeid(*jsonVal).name() : "null") << std::endl;
		}
	} else if constexpr (std::is_base_of_v<Reflectable, T>) {
		if (auto* obj = dynamic_cast<JSONObject*>(jsonVal)) {
			for (const auto& [name, value] : obj->properties) {
				member.set_member_value(name, value.get());
			}
		} else {
			std::cerr << "Fehler: Erwartete JSONObject für Reflectable-Typ, bekam "
					  << (jsonVal ? typeid(*jsonVal).name() : "null") << std::endl;
		}
	} else {
		std::cerr << "Warnung: Keine spezifische assign_from_json für Typ "
				  << typeid(T).name() << " und JSON-Typ "
				  << (jsonVal ? typeid(*jsonVal).name() : "null") << std::endl;
	}
}

template<typename T>
void assign_from_json(std::vector<T>& member, JSONValue* jsonVal) {
	member.clear();
	if (auto* arr = dynamic_cast<JSONArray*>(jsonVal)) {
		for (const auto& elem : arr->elements) {
			T item{};
			assign_from_json(item, elem.get());
			member.push_back(std::move(item));
		}
	} else if constexpr (std::is_same_v<T, std::string>) {
		if (auto* sv = dynamic_cast<JSONString*>(jsonVal)) {
			member.push_back(sv->value);
		} else {
			std::cerr << "Fehler: Erwartete JSONArray oder JSONString für std::vector<std::string>, bekam "
					  << (jsonVal ? typeid(*jsonVal).name() : "null") << std::endl;
		}
	} else {
		std::cerr << "Fehler: Erwartete JSONArray für std::vector<T>, bekam "
				  << (jsonVal ? typeid(*jsonVal).name() : "null") << std::endl;
	}
}

template<typename T>
void assign_from_json(std::optional<T>& member, JSONValue* jsonVal) {
	if (dynamic_cast<JSONNull*>(jsonVal)) {
		member.reset();
		return;
	}

	T value{};
	assign_from_json(value, jsonVal);
	member = std::move(value);
}
