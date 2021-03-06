#include "keychain.h"

VALUE cKeychainItem;
VALUE NullCodeStr;

static void keychain_item_free(struct KeychainItem *item)
{
	if (item->itemRef)
		CFRelease(item->itemRef);
}

static void keychain_item_mark(struct KeychainItem *item)
{
}

static VALUE keychain_item_alloc(VALUE klass)
{
	struct KeychainItem *keychain_item;
	return Data_Make_Struct(klass, struct KeychainItem, keychain_item_mark, keychain_item_free, keychain_item);
}


static VALUE keychain_item_delete(VALUE self)
{
	SecKeychainItemRef *itemRef = KEYCHAIN_ITEM(self)->itemRef;
	SecKeychainItemDelete(itemRef);
	CFRelease(itemRef);
	KEYCHAIN_ITEM(self)->itemRef = NULL;
	return Qnil;
}


static VALUE keychain_item_get_attribute(VALUE self, SecItemAttr attr)
{
	SecKeychainItemRef *itemRef = KEYCHAIN_ITEM(self)->itemRef;

	SecKeychainAttribute attributes[1];
	attributes[0].tag = attr;

	SecKeychainAttributeList list;
	list.count = 1;
	list.attr  = attributes;

	OSStatus error = SecKeychainItemCopyContent(itemRef, NULL, &list, 0, NULL);

	if (error == errSecNoSuchAttr) {
		return Qnil;
	} else if (error) {
		keychain_raise_error(error);
	} else {
		VALUE result = rb_str_new(attributes[0].data, attributes[0].length);
		SecKeychainItemFreeContent(&list, NULL);
		return result;
	}
}


static VALUE keychain_item_creation_date(VALUE self)
{
	VALUE time_str = keychain_item_get_attribute(self, kSecCreationDateItemAttr);
	VALUE time_obj = rb_funcall(rb_cTime, rb_intern("parse"), 1, time_str);
	return time_obj;
}

static VALUE keychain_item_modified_date(VALUE self)
{
	VALUE time_str = keychain_item_get_attribute(self, kSecModDateItemAttr);
	VALUE time_obj = rb_funcall(rb_cTime, rb_intern("parse"), 1, time_str);
	return time_obj;
}

static VALUE keychain_item_kind(VALUE self)
{
	VALUE str = keychain_item_get_attribute(self, kSecDescriptionItemAttr);
	KEYCHAIN_SET_UTF8_ENCODING(str);
	return str;
}

static VALUE keychain_item_comment(VALUE self)
{
	VALUE str = keychain_item_get_attribute(self, kSecCommentItemAttr);
	KEYCHAIN_SET_UTF8_ENCODING(str);
	return str;
}


static VALUE keychain_item_creator(VALUE self)
{
	VALUE str = keychain_item_get_attribute(self, kSecCreatorItemAttr);
	KEYCHAIN_SET_BINARY_ENCODING(str);
	return str;
}

static VALUE keychain_item_type(VALUE self)
{
	VALUE bigendian_str = keychain_item_get_attribute(self, kSecTypeItemAttr);

	if (bigendian_str == Qnil) {
		return Qnil;
	} else {
		KEYCHAIN_SET_BINARY_ENCODING(bigendian_str);
		return rb_funcall(bigendian_str, rb_intern("reverse"), 0);
	}
}

static VALUE keychain_item_label(VALUE self)
{
	VALUE str = keychain_item_get_attribute(self, kSecLabelItemAttr);
	KEYCHAIN_SET_UTF8_ENCODING(str);
	return str;
}

static VALUE keychain_item_account(VALUE self)
{
	VALUE str = keychain_item_get_attribute(self, kSecAccountItemAttr);
	KEYCHAIN_SET_UTF8_ENCODING(str);
	return str;
}

static VALUE keychain_item_service(VALUE self)
{
	VALUE str = keychain_item_get_attribute(self, kSecServiceItemAttr);
	KEYCHAIN_SET_UTF8_ENCODING(str);
	return str;
}

static VALUE keychain_item_value(VALUE self)
{
	VALUE str = keychain_item_get_attribute(self, kSecGenericItemAttr);
	KEYCHAIN_SET_BINARY_ENCODING(str);
	return str;
}

static VALUE keychain_item_security_domain(VALUE self)
{
	VALUE str = keychain_item_get_attribute(self, kSecSecurityDomainItemAttr);
	KEYCHAIN_SET_UTF8_ENCODING(str);
	return str;
}

static VALUE keychain_item_server(VALUE self)
{
	VALUE str = keychain_item_get_attribute(self, kSecServerItemAttr);
	KEYCHAIN_SET_UTF8_ENCODING(str);
	return str;
}


static VALUE keychain_item_authentication_type(VALUE self)
{
	VALUE charcode = keychain_item_get_attribute(self, kSecAuthenticationTypeItemAttr);
	if (rb_funcall(charcode, rb_intern("eql?"), 1, NullCodeStr) == Qtrue) {
		return Qnil;
	} else {
		KEYCHAIN_SET_BINARY_ENCODING(charcode);
		return charcode;
	}
}

static VALUE keychain_item_port(VALUE self)
{
	VALUE integer_str = keychain_item_get_attribute(self, kSecPortItemAttr);
	VALUE integer_obj = rb_funcall(integer_str, rb_intern("to_i"), 0);
	return integer_obj;
}

static VALUE keychain_item_path(VALUE self)
{
	VALUE str = keychain_item_get_attribute(self, kSecPathItemAttr);
	KEYCHAIN_SET_UTF8_ENCODING(str);
	return str;
}

static VALUE keychain_item_protocol(VALUE self)
{
	VALUE bigendian_str = keychain_item_get_attribute(self, kSecProtocolItemAttr);

	if (rb_funcall(bigendian_str, rb_intern("eql?"), 1, NullCodeStr) == Qtrue) {
		return Qnil;
	} else if (bigendian_str == Qnil) {
		return Qnil;
	} else {
		KEYCHAIN_SET_BINARY_ENCODING(bigendian_str);
		return rb_funcall(bigendian_str, rb_intern("reverse"), 0);
	}
}

static VALUE keychain_item_password(VALUE self)
{
	SecKeychainItemRef *itemRef = KEYCHAIN_ITEM(self)->itemRef;

	UInt32 passwordLength;
	void *passwordData;

	OSStatus error = SecKeychainItemCopyContent(itemRef, NULL, NULL, &passwordLength, &passwordData);
	if (error) keychain_raise_error(error);

	VALUE result = rb_str_new(passwordData, passwordLength);
	SecKeychainItemFreeContent(NULL, passwordData);
	KEYCHAIN_SET_BINARY_ENCODING(result);
	return result;
}

static VALUE keychain_item_inspect(VALUE self)
{
	VALUE str = KEYCHAIN_ASCII_STR_NEW2("#<Keychain::Item \"");
	rb_str_append(str, keychain_item_label(self)); /* ASCII + UTF-8 (compatible) */
	rb_str_append(str, KEYCHAIN_ASCII_STR_NEW2("\">"));
	return str;
}


static VALUE keychain_item_to_hash(VALUE self)
{
	VALUE hash = rb_hash_new();

	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("creation_date"), keychain_item_creation_date(self));
	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("modified_date"), keychain_item_modified_date(self));
	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("kind"), keychain_item_kind(self));
	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("comment"), keychain_item_comment(self));
	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("creator"), keychain_item_creator(self));
	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("type"), keychain_item_type(self));
	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("label"), keychain_item_label(self));
	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("account"), keychain_item_account(self));
	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("service"), keychain_item_service(self));
	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("value"), keychain_item_value(self));
	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("security_domain"), keychain_item_security_domain(self));
	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("server"), keychain_item_server(self));
	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("authentication_type"), keychain_item_authentication_type(self));
	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("port"), keychain_item_port(self));
	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("path"), keychain_item_path(self));
	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("protocol"), keychain_item_protocol(self));

	rb_hash_aset(hash, KEYCHAIN_ASCII_STR_NEW2("password"), keychain_item_password(self));

	return hash;
}

static VALUE keychain_item_to_json(int argc, VALUE *argv, VALUE self)
{
	return rb_funcall2(keychain_item_to_hash(self), rb_intern("to_json"), argc, argv);
}

static VALUE keychain_item_to_yaml(int argc, VALUE *argv, VALUE self)
{
	return rb_funcall2(keychain_item_to_hash(self), rb_intern("to_yaml"), argc, argv);
}


void Init_keychain_item()
{
	rb_require("time");

	VALUE mKeychain;
	mKeychain = rb_define_module("Keychain");
	cKeychainItem = rb_define_class_under(mKeychain, "Item", rb_cObject);

	rb_define_alloc_func(cKeychainItem, keychain_item_alloc);

	rb_define_method(cKeychainItem, "delete", keychain_item_delete, 0);

	char null[] = {0, 0, 0, 0};
	NullCodeStr = rb_str_new(null, 4);

	rb_define_method(cKeychainItem, "creation_date", keychain_item_creation_date, 0);
	rb_define_method(cKeychainItem, "modified_date", keychain_item_modified_date, 0);
	rb_define_method(cKeychainItem, "kind", keychain_item_kind, 0);
	rb_define_method(cKeychainItem, "comment", keychain_item_comment, 0);
	rb_define_method(cKeychainItem, "creator", keychain_item_creator, 0);
	rb_define_method(cKeychainItem, "type", keychain_item_type, 0);
	rb_define_method(cKeychainItem, "label", keychain_item_label, 0);
	rb_define_method(cKeychainItem, "account", keychain_item_account, 0);
	rb_define_method(cKeychainItem, "service", keychain_item_service, 0);
	rb_define_method(cKeychainItem, "value", keychain_item_value, 0);
	rb_define_method(cKeychainItem, "security_domain", keychain_item_security_domain, 0);
	rb_define_method(cKeychainItem, "server", keychain_item_server, 0);
	rb_define_method(cKeychainItem, "authentication_type", keychain_item_authentication_type, 0);
	rb_define_method(cKeychainItem, "port", keychain_item_port, 0);
	rb_define_method(cKeychainItem, "path", keychain_item_path, 0);
	rb_define_method(cKeychainItem, "protocol", keychain_item_protocol, 0);

	rb_define_method(cKeychainItem, "password", keychain_item_password, 0);

	rb_define_method(cKeychainItem, "inspect", keychain_item_inspect, 0);
	rb_define_method(cKeychainItem, "to_json", keychain_item_to_json, -1);
	rb_define_method(cKeychainItem, "to_yaml", keychain_item_to_yaml, -1);
}
