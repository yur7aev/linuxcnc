#include "msg_log.h"
#include <string.h>
#include <time.h>

std::list<Message> messages;
bool messages_visible = false;

int get_messages()
{
	NMLTYPE type;
	static char t[LINELEN + 1];

	if (emcErrorBuffer == NULL || !emcErrorBuffer->valid()) return -1;

	switch (type = emcErrorBuffer->read()) {
	case EMC_OPERATOR_ERROR_TYPE:
		strncpy(t, ((EMC_OPERATOR_ERROR *)(emcErrorBuffer->get_address()))->error, LINELEN - 1);
		break;
	case EMC_OPERATOR_TEXT_TYPE:
		strncpy(t, ((EMC_OPERATOR_TEXT *) (emcErrorBuffer->get_address()))->text, LINELEN - 1);
		break;
	case EMC_OPERATOR_DISPLAY_TYPE:
		strncpy(t, ((EMC_OPERATOR_DISPLAY *) (emcErrorBuffer->get_address()))->display, LINELEN - 1);
		break;
	case NML_ERROR_TYPE:
		strncpy(t, ((NML_ERROR *) (emcErrorBuffer->get_address()))->error, NML_ERROR_LEN - 1);
		break;
	case NML_TEXT_TYPE:
		strncpy(t, ((NML_TEXT *) (emcErrorBuffer->get_address()))->text, NML_TEXT_LEN - 1); 
		break;
	case NML_DISPLAY_TYPE:
		strncpy(t, ((NML_DISPLAY *) (emcErrorBuffer->get_address()))->display, NML_DISPLAY_LEN - 1);
		break;
	case  0:
		return 1;	// nothong new
	default:
		return -1;
	}

	t[LINELEN - 1] = 0;
	messages.push_front(Message(type, t));
	if (messages.size() > 100) messages.pop_back();
	return 0;
}
