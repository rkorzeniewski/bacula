<br/>
<div class='titlediv'>
  <h1 class='newstitle'> 
   Déplacer des médias
  </h1>
 </div>
 <div class='bodydiv'>

<form action="?" method='GET'>
<table>
<tr>
<td><b>To: </b></td><td><input class='formulaire' name='email' value='<TMPL_VAR email>'></td>
</tr><tr>
<td><b>Subject: </b></td><td><input class='formulaire' name='subject' value='[BACULA] Déplacer des médias vers <TMPL_VAR newlocation>' size='80'></td>
</tr><tr>
<td></td>
<td>
<textarea name='content' class='formulaire' cols='80' rows='32'>
Bonjour,

Pouvez vous déplacer ces médias vers <TMPL_VAR newlocation>
Media :
<TMPL_LOOP media>
 - <TMPL_VAR VolumeName>  (<TMPL_VAR location>)
</TMPL_LOOP>

Quand cela sera terminé, pouvez vous cliquer sur le lien ci-dessous pour mettre à jour la localisation ?
(Vous pouvez utiliser ce lien : <TMPL_VAR url>).

Merci
</textarea>
</td></tr></table>
<input class='formulaire' type='submit' name='action' value='move_email'>
</form>
<br>
<a href="<TMPL_VAR url>"><img alt='Mettre à jour maintenant' src='/bweb/update.png'>Mettre à jour maintenant</a>
</div>
