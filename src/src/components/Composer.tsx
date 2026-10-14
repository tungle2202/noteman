import { useState } from 'react';
import { Code, Image, Send, X } from 'lucide-react';
import { useI18n } from '../i18n/I18nProvider';

/** One input, two modes: new thread (no target) or a note inside an existing thread (target set). */
export function Composer({
  targetTitle,
  onCancelTarget,
  onSubmit,
}: {
  targetTitle?: string;
  onCancelTarget: () => void;
  onSubmit: (text: string) => void;
}) {
  const { t } = useI18n();
  const [text, setText] = useState('');
  const send = () => {
    if (text.trim()) {
      onSubmit(text);
      setText('');
    }
  };

  return (
    <div className="composer">
      {targetTitle !== undefined && (
        <div className="chip">
          <span>{t('composer.replyingTo', { title: targetTitle || t('thread.untitled') })}</span>
          <button className="ibtn" style={{ width: 20, height: 20 }} onClick={onCancelTarget} aria-label={t('composer.cancel')}>
            <X size={13} />
          </button>
        </div>
      )}
      <textarea
        value={text}
        rows={2}
        onChange={(e) => setText(e.target.value)}
        placeholder={targetTitle !== undefined ? t('composer.replyPlaceholder') : t('composer.newThread')}
        onKeyDown={(e) => {
          if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            send();
          }
        }}
      />
      <div className="bar">
        <button className="ibtn" title={t('composer.image')} aria-label={t('composer.image')}>
          <Image size={16} />
        </button>
        <button className="ibtn" title={t('composer.code')} aria-label={t('composer.code')}>
          <Code size={16} />
        </button>
        <button className="send" disabled={!text.trim()} onClick={send}>
          <span style={{ display: 'inline-flex', alignItems: 'center', gap: 6 }}>
            <span>{t('composer.send')}</span>
            <Send size={14} />
          </span>
        </button>
      </div>
    </div>
  );
}
