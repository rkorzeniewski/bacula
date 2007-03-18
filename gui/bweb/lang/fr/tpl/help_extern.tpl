<br/>
<div class='titlediv'>
 <h1 class='newstitle'> Assistant d'externalisation de médias (partie 1/2)</h1>
</div>
<div class='bodydiv'>
Cet outil va sélectionner pour vous les meilleurs cartouches à externaliser.
Vous devrez choisir parmi la sélection de l'écran suivant.

  <form action="?" method='GET'>
   <table>
    <tr><td>Pool :</td>      
        <td><select name='pool' class='formulaire' multiple>
<TMPL_LOOP NAME=db_pools>
             <option selected><TMPL_VAR NAME=name></option>
</TMPL_LOOP>
           </select>
        </td>
    </tr>
    <tr><td>Type de média:</td>      
        <td><select name='mediatype' class='formulaire' multiple>
<TMPL_LOOP NAME=db_mediatypes>
             <option><TMPL_VAR NAME=mediatype></option>
</TMPL_LOOP>
           </select>
        </td>
    </tr>
    <tr><td> Localisation : </td>
        <td><select name='location' class='formulaire'>
  <TMPL_LOOP NAME=db_locations>
      <option id='loc_<TMPL_VAR NAME=location>' value='<TMPL_VAR NAME=location>'><TMPL_VAR NAME=location></option>
  </TMPL_LOOP>
    </select>
        </td>
    </tr>
    <tr>
        <td>Nombre de médias <br/> à externaliser :</td> 
        <td> <input type='text' name='limit' size='3' class='formulaire' 
              value='10'> </td>
    </tr>
    <tr>
        <td><input type="image" name='action' value='compute_extern_media' title='Suivant' src='/bweb/next.png'>
        </td><td/>
    </tr>
   </table>
   </form>
</div>
